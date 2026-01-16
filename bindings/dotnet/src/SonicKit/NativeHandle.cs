using System;

namespace SonicKit;

/// <summary>
/// Base class for native handle wrappers with automatic disposal.
/// </summary>
public abstract class NativeHandle : IDisposable
{
    /// <summary>
    /// Gets the underlying native handle.
    /// </summary>
    protected internal IntPtr Handle { get; private set; }

    /// <summary>
    /// Gets a value indicating whether this instance has been disposed.
    /// </summary>
    public bool IsDisposed => Handle == IntPtr.Zero;

    /// <summary>
    /// Initializes a new instance with the specified native handle.
    /// </summary>
    protected NativeHandle(IntPtr handle)
    {
        Handle = handle != IntPtr.Zero
            ? handle
            : throw new InvalidOperationException("Failed to create native handle.");
    }

    /// <summary>
    /// Ensures the handle is valid before use.
    /// </summary>
    protected void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(IsDisposed, this);
    }

    /// <summary>
    /// Override to release native resources.
    /// </summary>
    protected abstract void ReleaseHandle();

    /// <inheritdoc/>
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    /// <summary>
    /// Releases resources.
    /// </summary>
    protected virtual void Dispose(bool disposing)
    {
        if (Handle != IntPtr.Zero)
        {
            ReleaseHandle();
            Handle = IntPtr.Zero;
        }
    }

    /// <summary>
    /// Finalizer.
    /// </summary>
    ~NativeHandle()
    {
        Dispose(false);
    }
}
