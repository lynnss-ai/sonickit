using System;
using SonicKit.Native;

namespace SonicKit.Codecs;

/// <summary>
/// G.711 audio codec (A-law and μ-law).
/// </summary>
public sealed class G711Codec
{
    private readonly bool _useAlaw;

    /// <summary>
    /// Creates a new G.711 codec.
    /// </summary>
    /// <param name="useAlaw">True for A-law, false for μ-law.</param>
    public G711Codec(bool useAlaw = true)
    {
        _useAlaw = useAlaw;
    }

    /// <summary>
    /// Gets whether this codec uses A-law (vs μ-law).
    /// </summary>
    public bool IsAlaw => _useAlaw;

    /// <summary>
    /// Encode PCM samples to G.711.
    /// </summary>
    /// <param name="pcm">PCM samples (16-bit).</param>
    /// <returns>G.711 encoded bytes (1 byte per sample).</returns>
    public byte[] Encode(ReadOnlySpan<short> pcm)
    {
        var output = new byte[pcm.Length];
        Encode(pcm, output);
        return output;
    }

    /// <summary>
    /// Encode PCM samples to G.711.
    /// </summary>
    /// <param name="pcm">PCM samples.</param>
    /// <param name="output">Output buffer.</param>
    public void Encode(ReadOnlySpan<short> pcm, Span<byte> output)
    {
        if (output.Length < pcm.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (short* inPtr = pcm)
            fixed (byte* outPtr = output)
            {
                if (_useAlaw)
                    NativeMethods.voice_g711_encode_alaw((IntPtr)inPtr, (IntPtr)outPtr, pcm.Length);
                else
                    NativeMethods.voice_g711_encode_ulaw((IntPtr)inPtr, (IntPtr)outPtr, pcm.Length);
            }
        }
    }

    /// <summary>
    /// Decode G.711 bytes to PCM samples.
    /// </summary>
    /// <param name="encoded">G.711 encoded bytes.</param>
    /// <returns>PCM samples (16-bit).</returns>
    public short[] Decode(ReadOnlySpan<byte> encoded)
    {
        var output = new short[encoded.Length];
        Decode(encoded, output);
        return output;
    }

    /// <summary>
    /// Decode G.711 bytes to PCM samples.
    /// </summary>
    /// <param name="encoded">G.711 encoded bytes.</param>
    /// <param name="output">Output buffer.</param>
    public void Decode(ReadOnlySpan<byte> encoded, Span<short> output)
    {
        if (output.Length < encoded.Length)
            throw new ArgumentException("Output buffer too small.", nameof(output));

        unsafe
        {
            fixed (byte* inPtr = encoded)
            fixed (short* outPtr = output)
            {
                if (_useAlaw)
                    NativeMethods.voice_g711_decode_alaw((IntPtr)inPtr, (IntPtr)outPtr, encoded.Length);
                else
                    NativeMethods.voice_g711_decode_ulaw((IntPtr)inPtr, (IntPtr)outPtr, encoded.Length);
            }
        }
    }
}
