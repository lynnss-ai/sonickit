using System;
using SonicKit.Dsp;
using Xunit;

namespace SonicKit.Tests;

public class DspTests
{
    [Fact]
    public void Denoiser_Create_Succeeds()
    {
        using var denoiser = new Denoiser(16000, 160);
        Assert.Equal(160, denoiser.FrameSize);
    }

    [Fact]
    public void Denoiser_Process_ReturnsCorrectLength()
    {
        using var denoiser = new Denoiser(16000, 160);
        var input = new short[160];
        var output = denoiser.Process(input);
        Assert.Equal(160, output.Length);
    }

    [Fact]
    public void Denoiser_ProcessWithSpan_Works()
    {
        using var denoiser = new Denoiser(16000, 160);
        Span<short> input = stackalloc short[160];
        Span<short> output = stackalloc short[160];
        denoiser.Process(input, output);
    }

    [Fact]
    public void EchoCanceller_Create_Succeeds()
    {
        using var aec = new EchoCanceller(16000, 160);
        Assert.Equal(160, aec.FrameSize);
    }

    [Fact]
    public void EchoCanceller_Process_Works()
    {
        using var aec = new EchoCanceller(16000, 160);
        var captured = new short[160];
        var playback = new short[160];
        var output = aec.Process(captured, playback);
        Assert.Equal(160, output.Length);
    }

    [Fact]
    public void Agc_Create_Succeeds()
    {
        using var agc = new Agc(16000, 160);
    }

    [Fact]
    public void Agc_Process_Works()
    {
        using var agc = new Agc(16000, 160, Agc.Mode.Adaptive, -3f);
        var input = new short[160];
        var output = agc.Process(input);
        Assert.Equal(160, output.Length);
    }

    [Fact]
    public void Agc_GetGain_ReturnsValue()
    {
        using var agc = new Agc(16000, 160);
        var input = new short[160];
        agc.Process(input);
        float gain = agc.GetGain();
        Assert.True(!float.IsNaN(gain));
    }

    [Fact]
    public void Vad_Create_Succeeds()
    {
        using var vad = new Vad(16000);
    }

    [Fact]
    public void Vad_IsSpeech_WithSilence_ReturnsFalse()
    {
        using var vad = new Vad(16000, Vad.Mode.Quality);
        var silence = new short[160];
        bool isSpeech = vad.IsSpeech(silence);
        Assert.False(isSpeech);
    }

    [Fact]
    public void Vad_GetProbability_ReturnsBetweenZeroAndOne()
    {
        using var vad = new Vad(16000);
        var audio = new short[160];
        vad.IsSpeech(audio);
        float prob = vad.GetProbability();
        Assert.InRange(prob, 0f, 1f);
    }

    [Fact]
    public void Resampler_Create_Succeeds()
    {
        using var resampler = new Resampler(1, 16000, 48000);
    }

    [Fact]
    public void Resampler_Upsample_IncreasesLength()
    {
        using var resampler = new Resampler(1, 16000, 48000, 5);
        var input = new short[160];
        var output = resampler.Process(input);
        Assert.True(output.Length >= 480); // 3x upsample
    }

    [Fact]
    public void DtmfDetector_Create_Succeeds()
    {
        using var detector = new DtmfDetector(8000, 160);
    }

    [Fact]
    public void DtmfDetector_ProcessSilence_ReturnsNull()
    {
        using var detector = new DtmfDetector(8000, 160);
        var silence = new short[160];
        char digit = detector.Process(silence);
        Assert.Equal('\0', digit);
    }

    [Fact]
    public void DtmfGenerator_Create_Succeeds()
    {
        using var generator = new DtmfGenerator(8000, 100, 50);
    }

    [Fact]
    public void DtmfGenerator_GenerateDigit_ReturnsAudio()
    {
        using var generator = new DtmfGenerator(8000, 100, 50);
        var audio = generator.GenerateDigit('5');
        Assert.True(audio.Length > 0);
    }

    [Fact]
    public void DtmfGenerator_GenerateSequence_ReturnsAudio()
    {
        using var generator = new DtmfGenerator(8000, 100, 50);
        var audio = generator.GenerateSequence("123");
        Assert.True(audio.Length > 0);
    }

    [Fact]
    public void Equalizer_Create_Succeeds()
    {
        using var eq = new Equalizer(48000, 5);
    }

    [Fact]
    public void Equalizer_Process_Works()
    {
        using var eq = new Equalizer(48000, 5);
        eq.SetBand(0, 100, 3f, 1f);
        var input = new short[480];
        var output = eq.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void Compressor_Create_Succeeds()
    {
        using var comp = new Compressor(48000);
    }

    [Fact]
    public void Compressor_Process_Works()
    {
        using var comp = new Compressor(48000, -20f, 4f, 10f, 100f);
        var input = new short[480];
        var output = comp.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void ComfortNoiseGenerator_Create_Succeeds()
    {
        using var cng = new ComfortNoiseGenerator(16000, 160);
    }

    [Fact]
    public void ComfortNoiseGenerator_Generate_Works()
    {
        using var cng = new ComfortNoiseGenerator(16000, 160, -50f);
        var noise = cng.Generate(160);
        Assert.Equal(160, noise.Length);
    }
}
