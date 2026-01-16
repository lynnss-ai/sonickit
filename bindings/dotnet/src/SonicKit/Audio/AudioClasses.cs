using System;
using SonicKit.Native;

namespace SonicKit.Audio;

/// <summary>
/// Ring buffer for audio samples.
/// </summary>
public sealed class AudioBuffer : NativeHandle
{
    private readonly int _capacity;

    /// <summary>
    /// Creates a new audio buffer.
    /// </summary>
    /// <param name="capacity">Maximum number of samples.</param>
    /// <param name="channels">Number of audio channels.</param>
    public AudioBuffer(int capacity, int channels = 1)
        : base(NativeMethods.voice_buffer_create(capacity, channels))
    {
        _capacity = capacity;
    }

    /// <summary>
    /// Gets the buffer capacity.
    /// </summary>
    public int Capacity => _capacity;

    /// <summary>
    /// Gets the number of samples available for reading.
    /// </summary>
    public int Available
    {
        get
        {
            ThrowIfDisposed();
            return NativeMethods.voice_buffer_available(Handle);
        }
    }

    /// <summary>
    /// Gets the free space remaining.
    /// </summary>
    public int FreeSpace => _capacity - Available;

    /// <summary>
    /// Write samples to the buffer.
    /// </summary>
    /// <param name="samples">Samples to write.</param>
    /// <returns>Number of samples written.</returns>
    public int Write(ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                return NativeMethods.voice_buffer_write(Handle, (IntPtr)ptr, samples.Length);
            }
        }
    }

    /// <summary>
    /// Read and remove samples from the buffer.
    /// </summary>
    /// <param name="numSamples">Number of samples to read.</param>
    /// <returns>Read samples.</returns>
    public short[] Read(int numSamples)
    {
        ThrowIfDisposed();
        var output = new short[numSamples];
        int read = Read(output);
        return output.AsSpan(0, read).ToArray();
    }

    /// <summary>
    /// Read and remove samples from the buffer.
    /// </summary>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of samples read.</returns>
    public int Read(Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = output)
            {
                return NativeMethods.voice_buffer_read(Handle, (IntPtr)ptr, output.Length);
            }
        }
    }

    /// <summary>
    /// Clear all samples.
    /// </summary>
    public void Clear()
    {
        ThrowIfDisposed();
        NativeMethods.voice_buffer_clear(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_buffer_destroy(Handle);
    }
}

/// <summary>
/// Audio level meter.
/// </summary>
public sealed class AudioLevel : NativeHandle
{
    /// <summary>
    /// Creates a new level meter.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Frame size in samples.</param>
    public AudioLevel(int sampleRate, int frameSize)
        : base(NativeMethods.voice_level_create(sampleRate, frameSize))
    {
    }

    /// <summary>
    /// Process audio samples.
    /// </summary>
    public void Process(ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                NativeMethods.voice_level_process(Handle, (IntPtr)ptr, samples.Length);
            }
        }
    }

    /// <summary>
    /// Gets the current level in dB.
    /// </summary>
    public float GetLevelDb()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_level_get_db(Handle);
    }

    /// <summary>
    /// Gets the peak level in dB.
    /// </summary>
    public float GetPeakDb()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_level_get_peak_db(Handle);
    }

    /// <summary>
    /// Check if audio is silence.
    /// </summary>
    /// <param name="thresholdDb">Silence threshold in dB.</param>
    public bool IsSilence(float thresholdDb = -50f)
    {
        ThrowIfDisposed();
        return NativeMethods.voice_level_is_silence(Handle, thresholdDb);
    }

    /// <summary>
    /// Check if audio is clipping.
    /// </summary>
    public bool IsClipping()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_level_is_clipping(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_level_destroy(Handle);
    }
}

/// <summary>
/// Multi-channel audio mixer.
/// </summary>
public sealed class AudioMixer : NativeHandle
{
    private readonly int _numInputs;

    /// <summary>
    /// Creates a new mixer.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="numInputs">Number of input channels.</param>
    /// <param name="frameSize">Frame size in samples.</param>
    public AudioMixer(int sampleRate, int numInputs, int frameSize)
        : base(NativeMethods.voice_mixer_create(sampleRate, numInputs, frameSize))
    {
        _numInputs = numInputs;
    }

    /// <summary>
    /// Gets the number of input channels.
    /// </summary>
    public int NumInputs => _numInputs;

    /// <summary>
    /// Set input audio for a channel.
    /// </summary>
    /// <param name="index">Channel index.</param>
    /// <param name="samples">Audio samples.</param>
    public void SetInput(int index, ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        if (index < 0 || index >= _numInputs)
            throw new ArgumentOutOfRangeException(nameof(index));

        unsafe
        {
            fixed (short* ptr = samples)
            {
                NativeMethods.voice_mixer_set_input(Handle, index, (IntPtr)ptr, samples.Length);
            }
        }
    }

    /// <summary>
    /// Set gain for an input channel.
    /// </summary>
    /// <param name="index">Channel index.</param>
    /// <param name="gain">Gain (1.0 = unity).</param>
    public void SetInputGain(int index, float gain)
    {
        ThrowIfDisposed();
        if (index < 0 || index >= _numInputs)
            throw new ArgumentOutOfRangeException(nameof(index));

        NativeMethods.voice_mixer_set_input_gain(Handle, index, gain);
    }

    /// <summary>
    /// Mix all inputs.
    /// </summary>
    /// <param name="numSamples">Number of output samples.</param>
    /// <returns>Mixed audio.</returns>
    public short[] Mix(int numSamples)
    {
        ThrowIfDisposed();
        var output = new short[numSamples];
        Mix(output);
        return output;
    }

    /// <summary>
    /// Mix all inputs.
    /// </summary>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of samples written.</returns>
    public int Mix(Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = output)
            {
                return NativeMethods.voice_mixer_mix(Handle, (IntPtr)ptr, output.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_mixer_destroy(Handle);
    }
}

/// <summary>
/// Jitter buffer for network audio.
/// </summary>
public sealed class JitterBuffer : NativeHandle
{
    /// <summary>
    /// Creates a new jitter buffer.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSizeMs">Frame size in ms.</param>
    /// <param name="minDelayMs">Minimum buffer delay.</param>
    /// <param name="maxDelayMs">Maximum buffer delay.</param>
    public JitterBuffer(int sampleRate, int frameSizeMs = 20, int minDelayMs = 20, int maxDelayMs = 200)
        : base(NativeMethods.voice_jitter_create(sampleRate, frameSizeMs, minDelayMs, maxDelayMs))
    {
    }

    /// <summary>
    /// Put a packet into the jitter buffer.
    /// </summary>
    /// <param name="samples">Audio samples.</param>
    /// <param name="timestamp">RTP timestamp.</param>
    /// <param name="sequence">Sequence number.</param>
    public void Put(ReadOnlySpan<short> samples, uint timestamp, ushort sequence)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                NativeMethods.voice_jitter_put(Handle, (IntPtr)ptr, samples.Length, timestamp, sequence);
            }
        }
    }

    /// <summary>
    /// Get audio from the jitter buffer.
    /// </summary>
    /// <param name="numSamples">Number of samples to get.</param>
    /// <returns>Audio samples.</returns>
    public short[] Get(int numSamples)
    {
        ThrowIfDisposed();
        var output = new short[numSamples];
        int read = Get(output);
        return output.AsSpan(0, read).ToArray();
    }

    /// <summary>
    /// Get audio from the jitter buffer.
    /// </summary>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of samples written.</returns>
    public int Get(Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = output)
            {
                return NativeMethods.voice_jitter_get(Handle, (IntPtr)ptr, output.Length);
            }
        }
    }

    /// <summary>
    /// Gets the current buffer delay in milliseconds.
    /// </summary>
    public float GetDelayMs()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_jitter_get_delay_ms(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_jitter_destroy(Handle);
    }
}

/// <summary>
/// 3D spatial audio renderer.
/// </summary>
public sealed class SpatialRenderer : NativeHandle
{
    /// <summary>
    /// Creates a new spatial renderer.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Frame size in samples.</param>
    public SpatialRenderer(int sampleRate, int frameSize)
        : base(NativeMethods.voice_spatial_create(sampleRate, frameSize))
    {
    }

    /// <summary>
    /// Set the sound source position.
    /// </summary>
    public void SetSourcePosition(float x, float y, float z)
    {
        ThrowIfDisposed();
        NativeMethods.voice_spatial_set_source_position(Handle, x, y, z);
    }

    /// <summary>
    /// Set the listener position.
    /// </summary>
    public void SetListenerPosition(float x, float y, float z)
    {
        ThrowIfDisposed();
        NativeMethods.voice_spatial_set_listener_position(Handle, x, y, z);
    }

    /// <summary>
    /// Process mono audio to stereo with spatial positioning.
    /// </summary>
    /// <param name="input">Mono input samples.</param>
    /// <returns>Stereo output samples.</returns>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length * 2];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process mono audio to stereo with spatial positioning.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length * 2)
            throw new ArgumentException("Output buffer too small (needs 2x input for stereo).", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_spatial_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_spatial_destroy(Handle);
    }
}

/// <summary>
/// Head-Related Transfer Function processor.
/// </summary>
public sealed class Hrtf : NativeHandle
{
    /// <summary>
    /// Creates a new HRTF processor.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    public Hrtf(int sampleRate)
        : base(NativeMethods.voice_hrtf_create(sampleRate))
    {
    }

    /// <summary>
    /// Set the azimuth angle.
    /// </summary>
    /// <param name="azimuth">Azimuth in degrees (-180 to 180).</param>
    public void SetAzimuth(float azimuth)
    {
        ThrowIfDisposed();
        NativeMethods.voice_hrtf_set_azimuth(Handle, azimuth);
    }

    /// <summary>
    /// Set the elevation angle.
    /// </summary>
    /// <param name="elevation">Elevation in degrees (-90 to 90).</param>
    public void SetElevation(float elevation)
    {
        ThrowIfDisposed();
        NativeMethods.voice_hrtf_set_elevation(Handle, elevation);
    }

    /// <summary>
    /// Process mono audio to stereo with HRTF.
    /// </summary>
    /// <param name="input">Mono input samples.</param>
    /// <returns>Stereo output samples.</returns>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length * 2];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process mono audio to stereo with HRTF.
    /// </summary>
    public void Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        if (output.Length < input.Length * 2)
            throw new ArgumentException("Output buffer too small (needs 2x input for stereo).", nameof(output));

        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_hrtf_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_hrtf_destroy(Handle);
    }
}
