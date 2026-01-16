using System;
using SonicKit.Audio;
using SonicKit.Codecs;
using SonicKit.Effects;
using Xunit;

namespace SonicKit.Tests;

public class AudioTests
{
    [Fact]
    public void AudioBuffer_Create_Succeeds()
    {
        using var buffer = new AudioBuffer(4800);
        Assert.Equal(4800, buffer.Capacity);
        Assert.Equal(0, buffer.Available);
    }

    [Fact]
    public void AudioBuffer_WriteRead_Works()
    {
        using var buffer = new AudioBuffer(1000);
        var data = new short[] { 1, 2, 3, 4, 5 };
        buffer.Write(data);
        Assert.Equal(5, buffer.Available);

        var output = buffer.Read(3);
        Assert.Equal(3, output.Length);
        Assert.Equal(2, buffer.Available);
    }

    [Fact]
    public void AudioBuffer_Clear_Works()
    {
        using var buffer = new AudioBuffer(1000);
        buffer.Write(new short[100]);
        Assert.Equal(100, buffer.Available);

        buffer.Clear();
        Assert.Equal(0, buffer.Available);
    }

    [Fact]
    public void AudioLevel_Create_Succeeds()
    {
        using var level = new AudioLevel(16000, 160);
    }

    [Fact]
    public void AudioLevel_ProcessSilence_ReturnsLowLevel()
    {
        using var level = new AudioLevel(16000, 160);
        var silence = new short[160];
        level.Process(silence);

        float db = level.GetLevelDb();
        Assert.True(db < -50f);
    }

    [Fact]
    public void AudioLevel_IsSilence_Works()
    {
        using var level = new AudioLevel(16000, 160);
        var silence = new short[160];
        level.Process(silence);

        Assert.True(level.IsSilence());
    }

    [Fact]
    public void AudioMixer_Create_Succeeds()
    {
        using var mixer = new AudioMixer(16000, 4, 160);
        Assert.Equal(4, mixer.NumInputs);
    }

    [Fact]
    public void AudioMixer_Mix_Works()
    {
        using var mixer = new AudioMixer(16000, 2, 160);
        var audio1 = new short[160];
        var audio2 = new short[160];

        mixer.SetInput(0, audio1);
        mixer.SetInput(1, audio2);
        mixer.SetInputGain(0, 0.5f);

        var output = mixer.Mix(160);
        Assert.Equal(160, output.Length);
    }

    [Fact]
    public void JitterBuffer_Create_Succeeds()
    {
        using var jitter = new JitterBuffer(16000, 20, 40, 200);
    }

    [Fact]
    public void JitterBuffer_PutGet_Works()
    {
        using var jitter = new JitterBuffer(16000, 20, 40, 200);
        var audio = new short[320];

        jitter.Put(audio, 0, 0);
        jitter.Put(audio, 320, 1);

        var output = jitter.Get(320);
        Assert.Equal(320, output.Length);
    }

    [Fact]
    public void JitterBuffer_GetDelayMs_Works()
    {
        using var jitter = new JitterBuffer(16000, 20);
        float delay = jitter.GetDelayMs();
        Assert.True(delay >= 0f);
    }

    [Fact]
    public void SpatialRenderer_Create_Succeeds()
    {
        using var spatial = new SpatialRenderer(48000, 480);
    }

    [Fact]
    public void SpatialRenderer_Process_CreatesStereo()
    {
        using var spatial = new SpatialRenderer(48000, 480);
        spatial.SetSourcePosition(2f, 0f, 0f);

        var mono = new short[480];
        var stereo = spatial.Process(mono);

        Assert.Equal(960, stereo.Length); // 2x for stereo
    }

    [Fact]
    public void Hrtf_Create_Succeeds()
    {
        using var hrtf = new Hrtf(48000);
    }

    [Fact]
    public void Hrtf_Process_CreatesStereo()
    {
        using var hrtf = new Hrtf(48000);
        hrtf.SetAzimuth(90f);
        hrtf.SetElevation(0f);

        var mono = new short[480];
        var stereo = hrtf.Process(mono);

        Assert.Equal(960, stereo.Length);
    }
}

public class CodecTests
{
    [Fact]
    public void G711Codec_Create_Alaw()
    {
        var codec = new G711Codec(useAlaw: true);
        Assert.True(codec.IsAlaw);
    }

    [Fact]
    public void G711Codec_Create_Ulaw()
    {
        var codec = new G711Codec(useAlaw: false);
        Assert.False(codec.IsAlaw);
    }

    [Fact]
    public void G711Codec_EncodeDecode_Alaw()
    {
        var codec = new G711Codec(useAlaw: true);
        var original = new short[] { 0, 1000, -1000, 16000, -16000 };

        var encoded = codec.Encode(original);
        Assert.Equal(5, encoded.Length);

        var decoded = codec.Decode(encoded);
        Assert.Equal(5, decoded.Length);
    }

    [Fact]
    public void G711Codec_EncodeDecode_Ulaw()
    {
        var codec = new G711Codec(useAlaw: false);
        var original = new short[] { 0, 1000, -1000, 16000, -16000 };

        var encoded = codec.Encode(original);
        Assert.Equal(5, encoded.Length);

        var decoded = codec.Decode(encoded);
        Assert.Equal(5, decoded.Length);
    }
}

public class EffectsTests
{
    [Fact]
    public void Reverb_Create_Succeeds()
    {
        using var reverb = new Reverb(48000);
    }

    [Fact]
    public void Reverb_Process_Works()
    {
        using var reverb = new Reverb(48000, 0.7f, 0.5f, 0.3f, 0.7f);
        var input = new short[480];
        var output = reverb.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void Delay_Create_Succeeds()
    {
        using var delay = new Delay(48000);
    }

    [Fact]
    public void Delay_Process_Works()
    {
        using var delay = new Delay(48000, 250f, 0.4f, 0.5f);
        var input = new short[480];
        var output = delay.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void PitchShifter_Create_Succeeds()
    {
        using var shifter = new PitchShifter(48000);
    }

    [Fact]
    public void PitchShifter_Process_Works()
    {
        using var shifter = new PitchShifter(48000, 5f);
        var input = new short[480];
        var output = shifter.Process(input);
        Assert.True(output.Length > 0);
    }

    [Fact]
    public void Chorus_Create_Succeeds()
    {
        using var chorus = new Chorus(48000);
    }

    [Fact]
    public void Chorus_Process_Works()
    {
        using var chorus = new Chorus(48000, 1.5f, 0.5f, 0.5f);
        var input = new short[480];
        var output = chorus.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void Flanger_Create_Succeeds()
    {
        using var flanger = new Flanger(48000);
    }

    [Fact]
    public void Flanger_Process_Works()
    {
        using var flanger = new Flanger(48000, 0.5f, 0.5f, 0.5f, 0.5f);
        var input = new short[480];
        var output = flanger.Process(input);
        Assert.Equal(480, output.Length);
    }

    [Fact]
    public void TimeStretcher_Create_Succeeds()
    {
        using var stretcher = new TimeStretcher(48000);
    }

    [Fact]
    public void TimeStretcher_Process_Works()
    {
        using var stretcher = new TimeStretcher(48000, 1, 1.0f);
        var input = new short[480];
        var output = stretcher.Process(input);
        Assert.True(output.Length > 0);
    }

    [Fact]
    public void TimeStretcher_RateProperty_Works()
    {
        using var stretcher = new TimeStretcher(48000);
        stretcher.Rate = 0.5f;
        Assert.Equal(0.5f, stretcher.Rate);
    }

    [Fact]
    public void WatermarkEmbedder_Create_Succeeds()
    {
        using var embedder = new WatermarkEmbedder(48000);
    }

    [Fact]
    public void WatermarkEmbedder_EmbedString_Works()
    {
        using var embedder = new WatermarkEmbedder(48000, 0.15f);
        var audio = new short[48000];
        var marked = embedder.EmbedString(audio, "test");
        Assert.Equal(48000, marked.Length);
    }

    [Fact]
    public void WatermarkDetector_Create_Succeeds()
    {
        using var detector = new WatermarkDetector(48000);
    }

    [Fact]
    public void WatermarkDetector_HasWatermark_Works()
    {
        using var detector = new WatermarkDetector(48000);
        var audio = new short[48000];
        bool has = detector.HasWatermark(audio);
        // Just verify it doesn't throw
        Assert.True(true);
    }
}
