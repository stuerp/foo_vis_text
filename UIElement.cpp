
/** $VER: UIElement.cpp (2024.06.12) P. Stuer **/

#include "pch.h"

#include "UIElement.h"
#include "UIElementTracker.h"
#include "Encoding.h"
#include "Exceptions.h"
#include "Support.h"

#include <pathcch.h>
#pragma comment(lib, "pathcch")

#include <SDK/titleformat.h>
#include <SDK/playlist.h>
#include <SDK/playback_control.h>
#include <SDK/ui.h>

#pragma hdrstop

/// <summary>
/// Initializes a new instance.
/// </summary>
UIElement::UIElement() : m_bMsgHandled(FALSE)
{
    playlist_callback_single_impl_base::set_callback_flags(flag_on_item_focus_change);
}

#pragma region User Interface

/// <summary>
/// Creates the window.
/// </summary>
LRESULT UIElement::OnCreate(LPCREATESTRUCT cs) noexcept
{
    _UIElementTracker.Add(this);

    std::wstring WebViewVersion;

    if (!GetWebViewVersion(WebViewVersion))
    {
        console::printf(STR_COMPONENT_BASENAME " failed to find a compatible WebView component.");

        return 1;
    }

    console::printf(STR_COMPONENT_BASENAME " is using WebView %s.", WideToUTF8(WebViewVersion).c_str());

    _HostObject = Microsoft::WRL::Make<HostObject>
    (
        [this](std::function<void (void)> callback)
        {
            RunAsync(callback);
        }
    );

    Initialize();

    CreateWebView();

    InitializeFileWatcher();

    return 0;
}

/// <summary>
/// Destroys the window.
/// </summary>
void UIElement::OnDestroy() noexcept
{
    _FileWatcher.Stop();

    DeleteWebView();

    _HostObject = nullptr;

    _UIElementTracker.Remove(this);
}

/// <summary>
/// Handles the WM_SIZE message.
/// </summary>
void UIElement::OnSize(UINT type, CSize size) noexcept
{
    if (_Controller == nullptr)
        return;

    RECT Bounds;

    ::GetClientRect(m_hWnd, &Bounds);

    _Controller->put_Bounds(Bounds);
}

/// <summary>
/// Handles the WM_PAINT message.
/// </summary>
void UIElement::OnPaint(CDCHandle dc) noexcept
{
    PAINTSTRUCT ps = { };

    BeginPaint(&ps);

    RECT cr;

    GetClientRect(&cr);

    HTHEME hTheme = ::OpenThemeData(m_hWnd, VSCLASS_TEXTSTYLE);

    const DWORD Format = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

    if (hTheme != NULL)
    {
        ::DrawThemeText(hTheme, ps.hdc, TEXT_BODYTEXT, 0, TEXT(STR_COMPONENT_NAME), -1, Format, 0, &cr);

        ::CloseThemeData(hTheme);
    }
    else
        ::DrawTextW(ps.hdc, TEXT(STR_COMPONENT_NAME), -1, &cr, (UINT) Format);

    EndPaint(&ps);
}

/// <summary>
/// Handles a change to the template. Either the path name or the content changed.
/// </summary>
LRESULT UIElement::OnTemplateChanged(UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    try
    {
        InitializeWebView();
    }
    catch (std::exception & e)
    {
        console::error(e.what());
    }

    return 0;
}

/// <summary>
/// The WebView is ready.
/// </summary>
LRESULT UIElement::OnWebViewReady(UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    try
    {
        SetWebViewVisibility(IsWebViewVisible()); // Work-around for WebView not appearing after foobar2000 starts while being hosted in a hidden tab.

        InitializeWebView();
    }
    catch (std::exception & e)
    {
        console::error(e.what());
    }

    return 0;
}

/// <summary>
/// Handles a notification from the Preferences page that the template file path has changed.
/// </summary>
void UIElement::OnConfigurationChanged() noexcept
{
    _ExpandedTemplateFilePath = GetTemplateFilePath();

    try
    {
        InitializeFileWatcher();
        InitializeWebView();
    }
    catch (std::exception & e)
    {
        console::error(e.what());
    }
}

/// <summary>
/// Handles an async method call.
/// </summary>
LRESULT UIElement::OnAsync(UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    auto * task = reinterpret_cast<std::function<void()>*>(wParam);

    (*task)();

    delete task;

    return true;
}

/// <summary>
/// Initializes the component.
/// </summary>
void UIElement::Initialize()
{
    {
        pfc::string8 Path = pfc::io::path::combine(core_api::get_profile_path(), STR_COMPONENT_BASENAME);

        if (::_strnicmp(Path, "file://", 7) == 0)
            Path = Path.subString(7);

        _UserDataFolderPath = ::UTF8ToWide(Path.c_str());

        if (!::PathFileExistsW(_UserDataFolderPath.c_str()))
        {
            // Create the user data directory.
            if (!::CreateDirectoryW(_UserDataFolderPath.c_str(), nullptr))
                console::printf(::GetErrorMessage(::GetLastError(), ::FormatText(STR_COMPONENT_BASENAME " failed to create user data folder \"%s\"", ::WideToUTF8(_UserDataFolderPath).c_str())).c_str());
        }
    }

    {
        _ExpandedTemplateFilePath = GetTemplateFilePath();

        if (::PathFileExistsW(_ExpandedTemplateFilePath.c_str()))
            return;
    }

    // Create a default template file.
    {
        wchar_t DefaultFilePath[MAX_PATH];

        HMODULE hModule = GetCurrentModule();

        if (hModule == NULL)
            return;

        if (::GetModuleFileNameW(hModule, DefaultFilePath, _countof(DefaultFilePath)) == 0)
            return;

        HRESULT hResult = ::PathCchRemoveFileSpec(DefaultFilePath, _countof(DefaultFilePath));

        if (!SUCCEEDED(hResult))
            return;

        hResult = ::PathCchAppend(DefaultFilePath, _countof(DefaultFilePath), L"Default-Template.html");

        if (!SUCCEEDED(hResult))
            return;

        if (!::CopyFileW(DefaultFilePath, _ExpandedTemplateFilePath.c_str(), TRUE))
            console::printf(::GetErrorMessage(::GetLastError(), ::FormatText(STR_COMPONENT_BASENAME " failed to create default template file \"%s\"", ::WideToUTF8(_ExpandedTemplateFilePath).c_str())).c_str());
    }
}

/// <summary>
/// Initializes the file watcher.
/// </summary>
void UIElement::InitializeFileWatcher()
{
    _FileWatcher.Stop();

    try
    {
        _FileWatcher.Start(m_hWnd, _ExpandedTemplateFilePath);
    }
    catch (std::exception & e)
    {
        throw ComponentException(::FormatText("Failed to start file system watcher: %s", e.what()));
    }
}

/// <summary>
/// Initializes the WebView.
/// </summary>
void UIElement::InitializeWebView()
{
    if (_WebView == nullptr)
        return;

    // Navigate to the template.
    HRESULT hResult = _WebView->Navigate(_ExpandedTemplateFilePath.c_str());

    if (!SUCCEEDED(hResult))
    {
        (void)_WebView->Navigate(L"about:blank");

        throw Win32Exception(hResult, ::FormatText(STR_COMPONENT_BASENAME " failed to navigate to template \"%s\"", ::WideToUTF8(_ExpandedTemplateFilePath).c_str()));
    }

    on_playback_new_track(nullptr);
}

/// <summary>
/// Shows or hides the WebView.
/// </summary>
void UIElement::SetWebViewVisibility(bool visible) noexcept
{
    // Hack: Don't use put_IsVisible(). It hides the host window. Reduce the WebView's size if we need to show the client area 'behind' the WebView f.e. in Layout Edit mode.
    RECT cr = { };

    if (visible)
        GetClientRect(&cr);

    if (_Controller != nullptr)
        _Controller->put_Bounds(cr);

    if (visible)
        on_playback_new_track(nullptr); // Forces a refresh when the WebView becomes visible again e.g. after exiting Layout Edit mode.
    else
        InvalidateRect(nullptr, TRUE);
}

/// <summary>
/// Gets the template file path with all environment variables expanded.
/// </summary>
std::wstring UIElement::GetTemplateFilePath() const noexcept
{
    wchar_t FilePath[MAX_PATH];

    if (::ExpandEnvironmentStringsW(_Configuration._TemplateFilePath.c_str(), FilePath, _countof(FilePath)) == 0)
        ::wcscpy_s(FilePath, _countof(FilePath), _Configuration._TemplateFilePath.c_str());

    return std::wstring(FilePath);
}

/// <summary>
/// Shows the preferences page.
/// </summary>
void UIElement::ShowPreferences() noexcept
{
    _UIElementTracker.SetCurrentElement(this);

    static constexpr GUID _GUID = GUID_PREFERENCES;

    static_api_ptr_t<ui_control> uc;

    uc->show_preferences(_GUID);
}

/// <summary>
/// Gets the window class definition.
/// </summary>
CWndClassInfo & UIElement::GetWndClassInfo()
{
    static ATL::CWndClassInfoW wci =
    {
        {
            sizeof(WNDCLASSEX),
            CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
            StartWindowProc,
            0, 0,
            NULL, // Instance,
            NULL, // Icon
            NULL, // Cursor
            (HBRUSH) COLOR_WINDOW, // Background brush
            NULL, // Menu
            TEXT(STR_WINDOW_CLASS_NAME), // Class name
            NULL // Small Icon
        },
        NULL, NULL, IDC_ARROW, TRUE, 0, L""
    };

    return wci;
}

#pragma endregion

#pragma region play_callback_impl_base

/// <summary>
/// Called when playback is being initialized.
/// </summary>
void UIElement::on_playback_starting(play_control::t_track_command command, bool paused)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackStartingCallback;

    if (FunctionName.empty())
        return;

    static const wchar_t * CommandName = L"Unknown";

    if (command == play_control::t_track_command::track_command_play) CommandName = L"Play"; else
    if (command == play_control::t_track_command::track_command_next) CommandName = L"Next"; else       // Plays the next track from the current playlist according to the current playback order.
    if (command == play_control::t_track_command::track_command_prev) CommandName = L"Prev"; else       // Plays the previous track from the current playlist according to the current playback order.
    if (command == play_control::t_track_command::track_command_rand) CommandName = L"Random"; else     // Plays a random track from the current playlist.

    if (command == play_control::t_track_command::track_command_rand) CommandName = L"Set track"; else  // For internal use only, do not use.
    if (command == play_control::t_track_command::track_command_rand) CommandName = L"Resume";          // For internal use only, do not use.

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(\"%s\", %s)", FunctionName.c_str(), CommandName, (paused ? L"true" : L"false")).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_new_track() failed");
}

/// <summary>
/// Called when playback advances to a new track.
/// </summary>
void UIElement::on_playback_new_track(metadb_handle_ptr /*track*/)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackNewTrackCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s()", FunctionName.c_str()).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_new_track() failed");
}

/// <summary>
/// Called when playback stops.
/// </summary>
void UIElement::on_playback_stop(play_control::t_stop_reason reason)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackStopCallback;

    if (FunctionName.empty())
        return;

    static const wchar_t * Reason = L"unknown";

    if (reason == play_control::t_stop_reason::stop_reason_user)                Reason = L"User"; else
    if (reason == play_control::t_stop_reason::stop_reason_eof)                 Reason = L"EOF"; else
    if (reason == play_control::t_stop_reason::stop_reason_starting_another)    Reason = L"Starting another"; else
    if (reason == play_control::t_stop_reason::stop_reason_shutting_down)       Reason = L"Shutting down";

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(\"%s\")", FunctionName.c_str(), Reason).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_stop() failed");
}

/// <summary>
/// Called when the user seeks to a specific time.
/// </summary>
void UIElement::on_playback_seek(double time)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackSeekCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(%f)", FunctionName.c_str(), time).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_seek() failed");
}

/// <summary>
/// Called when playback pauses or resumes.
/// </summary>
void UIElement::on_playback_pause(bool paused)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackPauseCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(%s)", FunctionName.c_str(), (paused ? L"true" : L"false")).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_pause() failed");
}

/// <summary>
/// Called when the currently played file gets edited.
/// </summary>
void UIElement::on_playback_edited(metadb_handle_ptr hTrack)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackEditedCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s()", FunctionName.c_str()).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_edited() failed");
}

/// <summary>
/// Called when dynamic info (VBR bitrate etc...) changes.
/// </summary>
void UIElement::on_playback_dynamic_info(const file_info & fileInfo)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackDynamicInfoCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s()", FunctionName.c_str()).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_dynamic_info() failed");
}

/// <summary>
/// Called when the per-track dynamic info (stream track titles etc...) change. Happens less often than on_playback_dynamic_info().
/// </summary>
void UIElement::on_playback_dynamic_info_track(const file_info & fileInfo)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackDynamicTrackInfoCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s()", FunctionName.c_str()).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_dynamic_info_track() failed");
}

/// <summary>
/// Called, every second, for time display.
/// </summary>
void UIElement::on_playback_time(double time)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaybackTimeCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(%f)", FunctionName.c_str(), time).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_playback_time failed()");
}

/// <summary>
/// Called when the user changes the volume.
/// </summary>
void UIElement::on_volume_change(float newValue) // in dBFS
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnVolumeChangeCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s(%f)", FunctionName.c_str(), (double) newValue).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_volume_change failed()");
}

#pragma endregion

#pragma region playlist_callback_single

/// <summary>
/// Called when the selected item changes.
/// </summary>
void UIElement::on_item_focus_change(t_size fromIndex, t_size toIndex)
{
    if (_WebView == nullptr)
        return;

    const std::wstring FunctionName = OnPlaylistFocusedItemChangedCallback;

    if (FunctionName.empty())
        return;

    HRESULT hResult = _WebView->ExecuteScript(::FormatText(L"%s()", FunctionName.c_str()).c_str(), nullptr);

    if (!SUCCEEDED(hResult))
        throw Win32Exception(hResult, "on_item_focus_change failed()");
}

#pragma endregion
