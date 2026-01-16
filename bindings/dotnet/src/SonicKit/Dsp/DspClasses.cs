using System;
using System.Runtime.InteropServices;
using SonicKit.Native;

namespace SonicKit.Dsp;

/// <summary>
/// Noise reduction processor using spectral subtraction or RNNoise.
/// </summary>
public sealed class Denoiser : NativeHandle
{
    private readonly int _frameSize;

    /// <summary>
    /// Engine type for noise reduction.
    /// </summary>
    public enum EngineType
    {
        /// <summary>Auto-select best engine.</summary>
        Auto = 0,
        /// <summary>SpeexDSP engine.</summary>
        SpeexDsp = 1,
        /// <summary>RNNoise deep learning engine.</summary>
        RNNoise = 2
    }

    /// <summary>
    /// Creates a new denoiser.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Number of samples per frame.</param>
    /// <param name="engineType">Engine type to use.</param>
    public Denoiser(int sampleRate, int frameSize, EngineType engineType = EngineType.Auto)
        : base(NativeMethods.voice_denoiser_create(sampleRate, frameSize, (int)engineType))
    {
        _frameSize = frameSize;
    }

    /// <summary>
    /// Gets the frame size.
    /// </summary>
    public int FrameSize => _frameSize;

    /// <summary>
    /// Process audio frame for noise reduction.
    /// </summary>
    /// <param name="input">Input audio samples.</param>
    /// <returns>Denoised audio samples.</returns>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio frame for noise reduction.
    /// </summary>
    /// <param name="input">Input audio samples.</param>
    /// <param name="output">Output buffer for denoised samples.</param>
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
                NativeMethods.voice_denoiser_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Reset the denoiser state.
    /// </summary>
    public void Reset()
    {
        ThrowIfDisposed();
        NativeMethods.voice_denoiser_reset(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_denoiser_destroy(Handle);
    }
}

/// <summary>
/// Acoustic Echo Canceller for full-duplex communication.
/// </summary>
public sealed class EchoCanceller : NativeHandle
{
    private readonly int _frameSize;

    /// <summary>
    /// Creates a new echo canceller.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Number of samples per frame.</param>
    /// <param name="filterLength">Echo tail length in samples.</param>
    public EchoCanceller(int sampleRate, int frameSize, int filterLength = 2000)
        : base(NativeMethods.voice_aec_create(sampleRate, frameSize, filterLength))
    {
        _frameSize = frameSize;
    }

    /// <summary>
    /// Gets the frame size.
    /// </summary>
    public int FrameSize => _frameSize;

    /// <summary>
    /// Process captured audio with echo cancellation.
    /// </summary>
    /// <param name="captured">Microphone input.</param>
    /// <param name="playback">Speaker output reference.</param>
    /// <returns>Echo-cancelled audio.</returns>
    public short[] Process(ReadOnlySpan<short> captured, ReadOnlySpan<short> playback)
    {
        ThrowIfDisposed();
        var output = new short[captured.Length];
        Process(captured, playback, output);
        return output;
    }

    /// <summary>
    /// Process captured audio with echo cancellation.
    /// </summary>
    public void Process(ReadOnlySpan<short> captured, ReadOnlySpan<short> playback, Span<short> output)
    {
        ThrowIfDisposed();
        if (playback.Length < captured.Length)
            throw new ArgumentException("Playback buffer must match captured length.", nameof(playback));
        if (output.Length < captured.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* capPtr = captured)
            fixed (short* playPtr = playback)
            fixed (short* outPtr = output)
            {
                NativeMethods.voice_aec_process(Handle, (IntPtr)capPtr, (IntPtr)playPtr, (IntPtr)outPtr, captured.Length);
            }
        }
    }

    /// <summary>
    /// Reset the AEC state.
    /// </summary>
    public void Reset()
    {
        ThrowIfDisposed();
        NativeMethods.voice_aec_reset(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_aec_destroy(Handle);
    }
}

/// <summary>
/// Automatic Gain Control for level normalization.
/// </summary>
public sealed class Agc : NativeHandle
{
    /// <summary>
    /// AGC operating mode.
    /// </summary>
    public enum Mode
    {
        /// <summary>Fixed digital gain.</summary>
        Fixed = 0,
        /// <summary>Adaptive gain control.</summary>
        Adaptive = 1,
        /// <summary>Digital gain only.</summary>
        Digital = 2
    }

    /// <summary>
    /// Creates a new AGC.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Number of samples per frame.</param>
    /// <param name="mode">AGC operating mode.</param>
    /// <param name="targetLevel">Target output level in dBFS.</param>
    public Agc(int sampleRate, int frameSize, Mode mode = Mode.Adaptive, float targetLevel = -3.0f)
        : base(NativeMethods.voice_agc_create(sampleRate, frameSize, (int)mode, targetLevel))
    {
    }

    /// <summary>
    /// Process audio frame with AGC.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio frame with AGC.
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
                NativeMethods.voice_agc_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Gets the current gain in dB.
    /// </summary>
    public float GetGain()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_agc_get_gain(Handle);
    }

    /// <summary>
    /// Reset the AGC state.
    /// </summary>
    public void Reset()
    {
        ThrowIfDisposed();
        NativeMethods.voice_agc_reset(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_agc_destroy(Handle);
    }
}

/// <summary>
/// Voice Activity Detection.
/// </summary>
public sealed class Vad : NativeHandle
{
    /// <summary>
    /// VAD operating mode.
    /// </summary>
    public enum Mode
    {
        /// <summary>Quality mode - fewer false negatives.</summary>
        Quality = 0,
        /// <summary>Low bitrate mode.</summary>
        LowBitrate = 1,
        /// <summary>Aggressive mode.</summary>
        Aggressive = 2,
        /// <summary>Very aggressive mode - fewer false positives.</summary>
        VeryAggressive = 3
    }

    /// <summary>
    /// Creates a new VAD.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="mode">VAD operating mode.</param>
    public Vad(int sampleRate, Mode mode = Mode.LowBitrate)
        : base(NativeMethods.voice_vad_create(sampleRate, (int)mode))
    {
    }

    /// <summary>
    /// Check if audio frame contains speech.
    /// </summary>
    /// <param name="samples">Audio samples to analyze.</param>
    /// <returns>True if speech is detected.</returns>
    public bool IsSpeech(ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                return NativeMethods.voice_vad_process(Handle, (IntPtr)ptr, samples.Length);
            }
        }
    }

    /// <summary>
    /// Gets the speech probability from the last processed frame.
    /// </summary>
    public float GetProbability()
    {
        ThrowIfDisposed();
        return NativeMethods.voice_vad_get_probability(Handle);
    }

    /// <summary>
    /// Reset the VAD state.
    /// </summary>
    public void Reset()
    {
        ThrowIfDisposed();
        NativeMethods.voice_vad_reset(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_vad_destroy(Handle);
    }
}

/// <summary>
/// High-quality sample rate converter.
/// </summary>
public sealed class Resampler : NativeHandle
{
    private readonly int _inRate;
    private readonly int _outRate;

    /// <summary>
    /// Creates a new resampler.
    /// </summary>
    /// <param name="channels">Number of audio channels.</param>
    /// <param name="inRate">Input sample rate.</param>
    /// <param name="outRate">Output sample rate.</param>
    /// <param name="quality">Quality level (0-10).</param>
    public Resampler(int channels, int inRate, int outRate, int quality = 5)
        : base(NativeMethods.voice_resampler_create(channels, inRate, outRate, quality))
    {
        _inRate = inRate;
        _outRate = outRate;
    }

    /// <summary>
    /// Resample audio data.
    /// </summary>
    /// <param name="input">Input samples.</param>
    /// <returns>Resampled output.</returns>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        int outCapacity = (int)((long)input.Length * _outRate / _inRate) + 100;
        var output = new short[outCapacity];
        int written = Process(input, output);
        return output.AsSpan(0, written).ToArray();
    }

    /// <summary>
    /// Resample audio data.
    /// </summary>
    /// <param name="input">Input samples.</param>
    /// <param name="output">Output buffer.</param>
    /// <returns>Number of output samples written.</returns>
    public int Process(ReadOnlySpan<short> input, Span<short> output)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* inPtr = input)
            fixed (short* outPtr = output)
            {
                return NativeMethods.voice_resampler_process(Handle, (IntPtr)inPtr, input.Length, (IntPtr)outPtr, output.Length);
            }
        }
    }

    /// <summary>
    /// Reset the resampler state.
    /// </summary>
    public void Reset()
    {
        ThrowIfDisposed();
        NativeMethods.voice_resampler_reset(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_resampler_destroy(Handle);
    }
}

/// <summary>
/// DTMF tone detector.
/// </summary>
public sealed class DtmfDetector : NativeHandle
{
    /// <summary>
    /// Creates a new DTMF detector.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Frame size in samples.</param>
    public DtmfDetector(int sampleRate, int frameSize)
        : base(NativeMethods.voice_dtmf_detector_create(sampleRate, frameSize))
    {
    }

    /// <summary>
    /// Process audio and detect DTMF digit.
    /// </summary>
    /// <param name="samples">Audio samples.</param>
    /// <returns>Detected digit character, or '\0' if none.</returns>
    public char Process(ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                byte digit = NativeMethods.voice_dtmf_detector_process(Handle, (IntPtr)ptr, samples.Length);
                return digit == 0 ? '\0' : (char)digit;
            }
        }
    }

    /// <summary>
    /// Gets all accumulated detected digits.
    /// </summary>
    public string GetDigits()
    {
        ThrowIfDisposed();
        IntPtr ptr = NativeMethods.voice_dtmf_detector_get_digits(Handle);
        return Marshal.PtrToStringAnsi(ptr) ?? string.Empty;
    }

    /// <summary>
    /// Clear accumulated digits.
    /// </summary>
    public void Clear()
    {
        ThrowIfDisposed();
        NativeMethods.voice_dtmf_detector_clear(Handle);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_dtmf_detector_destroy(Handle);
    }
}

/// <summary>
/// DTMF tone generator.
/// </summary>
public sealed class DtmfGenerator : NativeHandle
{
    private readonly int _sampleRate;
    private readonly int _toneDurationMs;
    private readonly int _pauseDurationMs;

    /// <summary>
    /// Creates a new DTMF generator.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="toneDurationMs">Tone duration in ms.</param>
    /// <param name="pauseDurationMs">Pause duration in ms.</param>
    public DtmfGenerator(int sampleRate, int toneDurationMs = 100, int pauseDurationMs = 50)
        : base(NativeMethods.voice_dtmf_generator_create(sampleRate, toneDurationMs, pauseDurationMs))
    {
        _sampleRate = sampleRate;
        _toneDurationMs = toneDurationMs;
        _pauseDurationMs = pauseDurationMs;
    }

    /// <summary>
    /// Generate audio for a single DTMF digit.
    /// </summary>
    /// <param name="digit">DTMF digit (0-9, *, #, A-D).</param>
    /// <returns>Generated audio samples.</returns>
    public short[] GenerateDigit(char digit)
    {
        ThrowIfDisposed();
        int capacity = _sampleRate * _toneDurationMs / 1000 + 100;
        var output = new short[capacity];

        unsafe
        {
            fixed (short* ptr = output)
            {
                int written = NativeMethods.voice_dtmf_generator_generate_digit(Handle, (byte)digit, (IntPtr)ptr, capacity);
                return output.AsSpan(0, written).ToArray();
            }
        }
    }

    /// <summary>
    /// Generate audio for a sequence of DTMF digits.
    /// </summary>
    /// <param name="digits">String of DTMF digits.</param>
    /// <returns>Generated audio samples.</returns>
    public short[] GenerateSequence(string digits)
    {
        ThrowIfDisposed();
        int capacity = _sampleRate * digits.Length * (_toneDurationMs + _pauseDurationMs) / 1000 + 100;
        var output = new short[capacity];

        unsafe
        {
            fixed (short* outPtr = output)
            {
                IntPtr digitsPtr = Marshal.StringToHGlobalAnsi(digits);
                try
                {
                    int written = NativeMethods.voice_dtmf_generator_generate_sequence(Handle, digitsPtr, (IntPtr)outPtr, capacity);
                    return output.AsSpan(0, written).ToArray();
                }
                finally
                {
                    Marshal.FreeHGlobal(digitsPtr);
                }
            }
        }
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_dtmf_generator_destroy(Handle);
    }
}

/// <summary>
/// Multi-band parametric equalizer.
/// </summary>
public sealed class Equalizer : NativeHandle
{
    /// <summary>
    /// Creates a new equalizer.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="numBands">Number of EQ bands.</param>
    public Equalizer(int sampleRate, int numBands = 5)
        : base(NativeMethods.voice_eq_create(sampleRate, numBands))
    {
    }

    /// <summary>
    /// Process audio through the equalizer.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the equalizer.
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
                NativeMethods.voice_eq_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Set EQ band parameters.
    /// </summary>
    /// <param name="band">Band index.</param>
    /// <param name="frequency">Center frequency in Hz.</param>
    /// <param name="gainDb">Gain in dB.</param>
    /// <param name="q">Q factor.</param>
    public void SetBand(int band, float frequency, float gainDb, float q)
    {
        ThrowIfDisposed();
        NativeMethods.voice_eq_set_band(Handle, band, frequency, gainDb, q);
    }

    /// <summary>
    /// Set master output gain.
    /// </summary>
    /// <param name="gainDb">Gain in dB.</param>
    public void SetMasterGain(float gainDb)
    {
        ThrowIfDisposed();
        NativeMethods.voice_eq_set_master_gain(Handle, gainDb);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_eq_destroy(Handle);
    }
}

/// <summary>
/// Dynamic range compressor.
/// </summary>
public sealed class Compressor : NativeHandle
{
    /// <summary>
    /// Creates a new compressor.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="thresholdDb">Threshold in dB.</param>
    /// <param name="ratio">Compression ratio.</param>
    /// <param name="attackMs">Attack time in ms.</param>
    /// <param name="releaseMs">Release time in ms.</param>
    public Compressor(int sampleRate, float thresholdDb = -20f, float ratio = 4f, float attackMs = 10f, float releaseMs = 100f)
        : base(NativeMethods.voice_compressor_create(sampleRate, thresholdDb, ratio, attackMs, releaseMs))
    {
    }

    /// <summary>
    /// Process audio through the compressor.
    /// </summary>
    public short[] Process(ReadOnlySpan<short> input)
    {
        ThrowIfDisposed();
        var output = new short[input.Length];
        Process(input, output);
        return output;
    }

    /// <summary>
    /// Process audio through the compressor.
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
                NativeMethods.voice_compressor_process(Handle, (IntPtr)inPtr, (IntPtr)outPtr, input.Length);
            }
        }
    }

    /// <summary>
    /// Set the compression threshold.
    /// </summary>
    public void SetThreshold(float thresholdDb)
    {
        ThrowIfDisposed();
        NativeMethods.voice_compressor_set_threshold(Handle, thresholdDb);
    }

    /// <summary>
    /// Set the compression ratio.
    /// </summary>
    public void SetRatio(float ratio)
    {
        ThrowIfDisposed();
        NativeMethods.voice_compressor_set_ratio(Handle, ratio);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_compressor_destroy(Handle);
    }
}

/// <summary>
/// Comfort Noise Generator for silence substitution.
/// </summary>
public sealed class ComfortNoiseGenerator : NativeHandle
{
    /// <summary>
    /// Creates a new comfort noise generator.
    /// </summary>
    /// <param name="sampleRate">Sample rate in Hz.</param>
    /// <param name="frameSize">Frame size in samples.</param>
    /// <param name="noiseLevelDb">Initial noise level in dB.</param>
    public ComfortNoiseGenerator(int sampleRate, int frameSize, float noiseLevelDb = -50f)
        : base(NativeMethods.voice_cng_create(sampleRate, frameSize, noiseLevelDb))
    {
    }

    /// <summary>
    /// Analyze background noise characteristics.
    /// </summary>
    public void Analyze(ReadOnlySpan<short> samples)
    {
        ThrowIfDisposed();
        unsafe
        {
            fixed (short* ptr = samples)
            {
                NativeMethods.voice_cng_analyze(Handle, (IntPtr)ptr, samples.Length);
            }
        }
    }

    /// <summary>
    /// Generate comfort noise samples.
    /// </summary>
    /// <param name="numSamples">Number of samples to generate.</param>
    /// <returns>Generated noise samples.</returns>
    public short[] Generate(int numSamples)
    {
        ThrowIfDisposed();
        var output = new short[numSamples];
        unsafe
        {
            fixed (short* ptr = output)
            {
                NativeMethods.voice_cng_generate(Handle, (IntPtr)ptr, numSamples);
            }
        }
        return output;
    }

    /// <summary>
    /// Set the noise level.
    /// </summary>
    public void SetLevel(float levelDb)
    {
        ThrowIfDisposed();
        NativeMethods.voice_cng_set_level(Handle, levelDb);
    }

    /// <inheritdoc/>
    protected override void ReleaseHandle()
    {
        NativeMethods.voice_cng_destroy(Handle);
    }
}
