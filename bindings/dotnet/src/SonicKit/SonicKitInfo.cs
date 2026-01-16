using System.Runtime.InteropServices;
using SonicKit.Native;

namespace SonicKit;

/// <summary>
/// SonicKit library information.
/// </summary>
public static class SonicKitInfo
{
    /// <summary>
    /// Gets the native library version string.
    /// </summary>
    public static string Version
    {
        get
        {
            IntPtr ptr = NativeMethods.voice_get_version_string();
            return Marshal.PtrToStringAnsi(ptr) ?? "unknown";
        }
    }

    /// <summary>
    /// Gets the version components.
    /// </summary>
    public static (int Major, int Minor, int Patch) GetVersion()
    {
        NativeMethods.voice_get_version(out int major, out int minor, out int patch);
        return (major, minor, patch);
    }
}
