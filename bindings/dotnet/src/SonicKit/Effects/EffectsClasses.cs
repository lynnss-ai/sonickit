using System;
using System.Runtime.InteropServices;
using SonicKit.Native;

namespace SonicKit.Effects;

/// <summary>
/// Room reverb effect.
/// </summary>
public sealed class Reverb : NativeHandle
{
    /// <summary>
    /// Creates a new reverb effect.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="roomSize">Room size (0.0-1.0).</param>
    /// <param name="damping">Damping (0.0-1.0).</param>
    /// <param name="wetLevel">Wet level (0.0-1.0).</param>
    /// <param name="dryLevel">Dry level (0.0-1.0).</param>
    public Reverb(int sampleRate, float roomSize = 0.5f, float damping = 0.5f, float wetLevel = 0.3f, float dryLevel = 0.7f)
        : base(NativeMethods.voice_reverb_create(sampleRate, roomSize, damping, wetLevel, dryLevel))
    {
    }

    /// <summary>
    /// Process audio through the reverb.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the reverb.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_reverb_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Set the room size.
    /// </summary>
    public void SetRoomSize(float roomSize)
    {
        ThrowIfDisposed();
        NativeMethods.voice_reverb_set_room_size(Handle, roomSize);
    }

    /// <summary>
    /// Set the damping.
    /// </summary>
    public void SetDamping(float damping)
    {
        ThrowIfDisposed();
        NativeMethods.voice_reverb_set_damping(Handle, damping);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_reverb_destroy(Handle);
    }
}

/// <summary>
/// Echo/delay effect.
/// </summary>
public sealed class Delay : NativeHandle
{
    /// <summary>
    /// Creates a new delay effect.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="delayMs">Delay time in ms.</param>
    /// <param name="feedback">Feedback amount (0.0-1.0).</param>
    /// <param name="mix">Wet/dry mix (0.0-1.0).</param>
    public Delay(int sampleRate, float delayMs = 250f, float feedback = 0.4f, float mix = 0.5f)
        : base(NativeMethods.voice_delay_create(sampleRate, delayMs, feedback, mix))
    {
    }

    /// <summary>
    /// Process audio through the delay.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the delay.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_delay_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Set the delay time.
    /// </summary>
    public void SetDelayTime(float delayMs)
    {
        ThrowIfDisposed();
        NativeMethods.voice_delay_set_time(Handle, delayMs);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_delay_destroy(Handle);
    }
}

/// <summary>
/// Pitch shifter effect.
/// </summary>
public sealed class PitchShifter : NativeHandle
{
    private readonly int _sampleRate;

    /// <summary>
    /// Creates a new pitch shifter.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="semitones">Initial pitch shift in semitones.</param>
    public PitchShifter(int sampleRate, float semitones = 0f)
        : base(NativeMethods.voice_pitch_create(sampleRate, semitones))
    {
        _sampleRate = sampleRate;
    }

    /// <summary>
    /// Process audio through the pitch shifter.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        // Pitch shifting may change length, allocate extra
        int capacity = input.Length * 2;
        var output = new short[capacity];
        int written = Process(input, output);
        return output.AsSpan(0, written).ToArray();
    }

    /// <summary>
    /// Process audio through the pitch shifter.
    /// </summary>
    /// <param name="input">Input samples.</param>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of samples written.</returns>
    public int Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                return NativeMethods.voice_pitch_process(Handle, (IntPtr)inPtr, input.Length, (IntPtr)outPtr, output.Length);
            }
        }
    }

    /// <summary>
    /// Set the pitch shift amount.
    /// </summary>
    public void SetShift(float semitones)
    {
        ThrowIfDisposed();
        NativeMethods.voice_pitch_set_shift(Handle, semitones);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_pitch_destroy(Handle);
    }
}

/// <summary>
/// Chorus effect.
/// </summary>
public sealed class Chorus : NativeHandle
{
    /// <summary>
    /// Creates a new chorus effect.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="rate">Modulation rate in Hz.</param>
    /// <param name="depth">Modulation depth (0.0-1.0).</param>
    /// <param name="mix">Wet/dry mix (0.0-1.0).</param>
    public Chorus(int sampleRate, float rate = 1.5f, float depth = 0.5f, float mix = 0.5f)
        : base(NativeMethods.voice_chorus_create(sampleRate, rate, depth, mix))
    {
    }

    /// <summary>
    /// Process audio through the chorus.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the chorus.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_chorus_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_chorus_destroy(Handle);
    }
}

/// <summary>
/// Flanger effect.
/// </summary>
public sealed class Flanger : NativeHandle
{
    /// <summary>
    /// Creates a new flanger effect.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="rate">Modulation rate in Hz.</param>
    /// <param name="depth">Modulation depth (0.0-1.0).</param>
    /// <param name="feedback">Feedback (-1.0 to 1.0).</param>
    /// <param name="mix">Wet/dry mix (0.0-1.0).</param>
    public Flanger(int sampleRate, float rate = 0.5f, float depth = 0.5f, float feedback = 0.5f, float mix = 0.5f)
        : base(NativeMethods.voice_flanger_create(sampleRate, rate, depth, feedback, mix))
    {
    }

    /// <summary>
    /// Process audio through the flanger.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the flanger.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_flanger_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_flanger_destroy(Handle);
    }
}

/// <summary>
/// Time stretcher for changing tempo without pitch change.
/// </summary>
public sealed class TimeStretcher : NativeHandle
{
    private float _rate;

    /// <summary>
    /// Creates a new time stretcher.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="channels">Number of audio channels.</param>
    /// <param name="initialRate">Initial time stretch rate.</param>
    public TimeStretcher(int sampleRate, int channels = 1, float initialRate = 1.0f)
        : base(NativeMethods.voice_stretch_create(sampleRate, channels, initialRate))
    {
        _rate = initialRate;
    }

    /// <summary>
    /// Gets or sets the time stretch rate.
    /// </summary>
    public float Rate
    {
        get => _rate;
        set
        {
            ThrowIfDisposed();
            NativeMethods.voice_stretch_set_rate(Handle, value);
            _rate = value;
        }
    }

    /// <summary>
    /// Process audio through the time stretcher.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        // Output length depends on rate
        int capacity = (int)(input.Length / _rate) + 100;
        var output = new short[capacity];
        int written = Process(input, output);
        return output.AsSpan(0, written).ToArray();
    }

    /// <summary>
    /// Process audio through the time stretcher.
    /// </summary>
    /// <param name="input">Input samples.</param>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of samples written.</returns>
    public int Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                return NativeMethods.voice_stretch_process(Handle, (IntPtr)inPtr, input.Length, (IntPtr)outPtr, output.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_stretch_destroy(Handle);
    }
}

/// <summary>
/// Audio watermark embedder.
/// </summary>
public sealed class WatermarkEmbedder : NativeHandle
{
    /// <summary>
    /// Creates a new watermark embedder.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="strength">Watermark strength (0.0-1.0).</param>
    public WatermarkEmbedder(int sampleRate, float strength = 0.1f)
        : base(NativeMethods.voice_watermark_embedder_create(sampleRate, strength))
    {
    }

    /// <summary>
    /// Embed a string message as watermark.
    /// </summary>
    /// <param name="audio">Input audio.</param>
    /// <param name="message">Message to embed.</param>
    /// <returns>Watermarked audio.</returns>
    public short[] EmbedString(ReadOnlySpan<short> audio, string message)
    {
        ThrowIfDisposed();
        var output = new short[audio.Length];
        EmbedString(audio, message, output);
        return output;
    }

    /// <summary>
    /// Embed a string message as watermark.
    /// </summary>
    public void EmbedString(ReadOnlySpan<short> audio, string message, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < audio.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = audio)
            fixed (short* outPtr = output)
            {
                IntPtr msgPtr = Marshal.StringToHGlobalAnsi(message);
                try
                {
                    NativeMethods.voice_watermark_embed_string(Handle, (IntPtr)inPtr, (IntPtr)outPtr, audio.Length, msgPtr);
                }
                finally
                {
                    Marshal.FreeHGlobal(msgPtr);
                }
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_watermark_embedder_destroy(Handle);
    }
}

/// <summary>
/// Audio watermark detector.
/// </summary>
public sealed class WatermarkDetector : NativeHandle
{
    /// <summary>
    /// Creates a new watermark detector.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    public WatermarkDetector(int sampleRate)
        : base(NativeMethods.voice_watermark_detector_create(sampleRate))
    {
    }

    /// <summary>
    /// Check if audio contains a watermark.
    /// </summary>
    /// <param name="audio">Audio samples to check.</param>
    /// <returns>True if watermark is detected.</returns>
    public bool HasWatermark(ReadOnlySpan<short> audio)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = audio)
            {
                return NativeMethods.voice_watermark_has_watermark(Handle, (IntPtr)ptr, audio.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_watermark_detector_destroy(Handle);
    }
}
