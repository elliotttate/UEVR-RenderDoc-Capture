#include <spdlog/spdlog.h>

#include <utility/Scan.hpp>
#include <utility/String.hpp>

#include <sdk/FRenderTargetPool.hpp>
#include <sdk/EngineModule.hpp>
#include <sdk/threading/RHIThreadWorker.hpp>

#include "../VR.hpp"
#include "../../utility/Logging.hpp"
#include "RenderTargetPoolHook.hpp"

RenderTargetPoolHook* g_hook{nullptr};

namespace {
// A pooled-render-target debug name is a wide ASCII-ish string ("SceneVelocity", "SceneDepthZ", ...).
// A MISREAD arg (e.g. an `Out` TRefCountPtr pointer interpreted as the name) lands on heap/object memory
// whose first UTF-16 units are virtually never a clean printable-ASCII, null-terminated string. We use
// this to detect WHICH argument slot actually holds the name across UE layout changes. SEH-guarded
// because a wrong slot may not even be a valid pointer (could be a small integer / flag).
bool is_plausible_rt_name(const wchar_t* p) {
    if (p == nullptr || reinterpret_cast<uintptr_t>(p) < 0x10000) {
        return false; // null or small-integer => not a string pointer
    }
    __try {
        const wchar_t c0 = p[0];
        if (c0 < L'A' || c0 > L'z') {
            return false; // names start with a letter (Scene*, GBuffer*, Velocity, ...)
        }
        for (int i = 1; i < 80; ++i) {
            const wchar_t c = p[i];
            if (c == 0) {
                return i >= 2; // printable + null-terminated within a sane length
            }
            if (c < 0x20 || c > 0x7E) {
                return false; // a non-printable unit => not a debug name
            }
        }
        return false; // too long without a terminator => not a name
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false; // bad pointer
    }
}
} // namespace

RenderTargetPoolHook::RenderTargetPoolHook() {
    g_hook = this;
}

void RenderTargetPoolHook::on_pre_engine_tick(sdk::UGameEngine* engine, float delta) {
    if (!m_attempted_hook && VR::get()->is_depth_enabled()) {
        m_wants_activate = true;
    }

    if (!m_attempted_hook && m_wants_activate) {
        m_attempted_hook = true;
        m_hooked = hook();
    }
}

bool RenderTargetPoolHook::hook() {
    SPDLOG_INFO("Attempting to hook RenderTargetPool::FindFreeElement");

    const auto is_ue5 = VR::get()->get_fake_stereo_hook()->has_double_precision();
    const auto find_free_element = sdk::FRenderTargetPool::get_find_free_element_fn(is_ue5);

    if (!find_free_element) {
        SPDLOG_ERROR("Failed to find FRenderTargetPool::FindFreeElement, cannot hook");
        return false;
    }

    /*if (VR::get()->get_fake_stereo_hook()->has_double_precision()) {
        spdlog::error("Render target pool hook is temporarily disabled on UE5, sorry :(");
        return false;
    }*/

    SPDLOG_INFO("Performing hook...");

    if (is_ue5) {
        m_find_free_element_hook = safetyhook::create_inline((void*)*find_free_element, find_free_element_hook_ue5);
    } else {
        m_find_free_element_hook = safetyhook::create_inline((void*)*find_free_element, find_free_element_hook);
    }

    if (m_find_free_element_hook) {
        SPDLOG_INFO("Successfully hooked RenderTargetPool::FindFreeElement");
    } else {
        SPDLOG_ERROR("Failed to hook RenderTargetPool::FindFreeElement");
    }

    return true;
}

void RenderTargetPoolHook::on_post_find_free_element(
    sdk::FRenderTargetPool* pool, 
    sdk::FPooledRenderTargetDesc* desc, 
    TRefCountPtr<IPooledRenderTarget>* out, 
    const wchar_t* name)
{
    // Right now we are only using this for depth
    // and on some games it will crash if we mess with anything
    // so, TODO: fix the games that crash with depth enabled
    if (!m_wants_activate) {
        std::scoped_lock _{g_hook->m_mutex};
        m_render_targets.clear();
        return;
    }

    if (name != nullptr) {
        //SPDLOG_INFO("FRenderTargetPool::FindFreeElement called with name {}", utility::narrow(name));

        std::scoped_lock _{g_hook->m_mutex};

        if (out != nullptr) {
            g_hook->m_render_targets[name] = out->reference;
        } else {
            g_hook->m_render_targets.erase(name);
        }

        if (!g_hook->m_seen_names.contains(name)) {
            g_hook->m_seen_names.insert(name);
            SPDLOG_INFO("FRenderTargetPool::FindFreeElement called with name {}", utility::narrow(name));
        }
    }
}

bool RenderTargetPoolHook::find_free_element_hook(
    sdk::FRenderTargetPool* pool, sdk::FRHICommandListBase* cmd_list,
    sdk::FPooledRenderTargetDesc* desc, TRefCountPtr<IPooledRenderTarget>* out,
    const wchar_t* name, 
    uintptr_t a6, uintptr_t a7, uintptr_t a8, uintptr_t a9, uintptr_t a10)
{
    SPDLOG_INFO_ONCE("FRenderTargetPool::FindFreeElement (UE4) called for the first time!");

    const auto result = g_hook->m_find_free_element_hook.call<bool>(pool, cmd_list, desc, out, name, a6, a7, a8, a9, a10);

    SPDLOG_INFO_ONCE("Finished calling FRenderTargetPool::FindFreeElement!");

    g_hook->on_post_find_free_element(pool, desc, out, name);

    return result;
}

bool RenderTargetPoolHook::find_free_element_hook_ue5(
    sdk::FRenderTargetPool* pool,
    sdk::FPooledRenderTargetDesc* desc,
    TRefCountPtr<IPooledRenderTarget>* out,
    const wchar_t* name,
    // these arent uintptrs, just defending against future changes to the size of the params
    uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8, uintptr_t a9, uintptr_t a10)
{
    SPDLOG_INFO_ONCE("FRenderTargetPool::FindFreeElement (UE5) called for the first time!");

    // LAYOUT AUTO-DETECT. UE 5.3+ re-added an `FRHICommandListBase&` as the FIRST explicit argument
    // (RenderTargetPool.h: `FindFreeElement(FRHICommandListBase&, const FRHITextureCreateInfo&, Out&, Name)`),
    // which shifts every later parameter down one slot vs UE 5.0-5.2 `(Desc, Out, Name)`. Under our 4-reg
    // mapping (pool=RCX, desc=RDX, out=R8, name=R9, a5=stack0) that means on 5.3+ the REAL args are:
    //   cmd_list = desc(RDX), desc = out(R8), out = name(R9), name = a5(stack0).
    // Without correcting this we read `Out` as the name on 5.5 -> mojibake, and every named lookup
    // (SceneVelocity / SceneDepthZ) misses -> the whole pipeline falls back to heuristics. Detect once by
    // which slot actually holds a valid RT-name string.
    static int s_has_cmd_list = -1; // -1 unknown, 0 = UE5.0-5.2 layout, 1 = UE5.3+ layout
    if (s_has_cmd_list < 0) {
        const bool a5_is_name = is_plausible_rt_name(reinterpret_cast<const wchar_t*>(a5));
        const bool r9_is_name = is_plausible_rt_name(name);
        if (a5_is_name && !r9_is_name) {
            s_has_cmd_list = 1;
            SPDLOG_INFO("[RTPoolHook] Detected UE5.3+ FindFreeElement layout (FRHICommandListBase& prepended)");
        } else if (r9_is_name) {
            s_has_cmd_list = 0;
            SPDLOG_INFO("[RTPoolHook] Detected UE5.0-5.2 FindFreeElement layout");
        }
        // else: neither looked like a name this call; leave unknown and retry on a later call.
    }

    // Pass EVERY received argument straight through (incl. a5, which the old code dropped) so the original
    // sees them in the exact registers/stack slots they arrived in, regardless of layout. Extra trailing
    // args are harmless for the shorter (5.0-5.2) signature.
    const auto result = g_hook->m_find_free_element_hook.call<bool>(pool, desc, out, name, a5, a6, a7, a8, a9, a10);

    SPDLOG_INFO_ONCE("Finished calling FRenderTargetPool::FindFreeElement! (UE5)");

    // Record under the correctly-identified Out/Name. on_post_find_free_element ignores `desc`.
    if (s_has_cmd_list == 1) {
        g_hook->on_post_find_free_element(pool, /*desc*/nullptr,
            reinterpret_cast<TRefCountPtr<IPooledRenderTarget>*>(const_cast<wchar_t*>(name)),
            reinterpret_cast<const wchar_t*>(a5));
    } else if (s_has_cmd_list == 0) {
        g_hook->on_post_find_free_element(pool, desc, out, name);
    }
    // unknown layout this call: skip recording (don't capture garbage); next call retries detection.

    return result;
}