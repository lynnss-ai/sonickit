/**
 * @file sonickit.d.ts
 * @brief TypeScript type definitions for SonicKit WebAssembly module
 * @author SonicKit Team
 * @version 1.0.0
 */

declare module 'sonickit' {
    /**
     * Creates and initializes the SonicKit WebAssembly module
     * @returns Promise that resolves to the SonicKit module instance
     */
    export function createSonicKit(): Promise<SonicKitModule>;

    export interface SonicKitModule {
        // ============================================
        // Core DSP Classes
        // ============================================

        /**
         * Noise reduction using spectral subtraction or RNNoise
         */
        Denoiser: DenoiserConstructor;

        /**
         * Acoustic Echo Canceller for full-duplex communication
         */
        EchoCanceller: EchoCancellerConstructor;

        /**
         * Automatic Gain Control for level normalization
         */
        AGC: AGCConstructor;

        /**
         * Sample rate converter with high quality resampling
         */
        Resampler: ResamplerConstructor;

        /**
         * Voice Activity Detection
         */
        VAD: VADConstructor;

        // ============================================
        // Extended DSP Classes
        // ============================================

        /**
         * DTMF tone detection for telephony applications
         */
        DTMFDetector: DTMFDetectorConstructor;

        /**
         * DTMF tone generation for telephony applications
         */
        DTMFGenerator: DTMFGeneratorConstructor;

        /**
         * Multi-band parametric equalizer
         */
        Equalizer: EqualizerConstructor;

        /**
         * Dynamic range compressor
         */
        Compressor: CompressorConstructor;

        /**
         * Peak limiter for preventing clipping
         */
        Limiter: LimiterConstructor;

        /**
         * Noise gate for reducing background noise
         */
        NoiseGate: NoiseGateConstructor;

        /**
         * Comfort Noise Generator for silence substitution
         */
        ComfortNoise: ComfortNoiseConstructor;

        /**
         * Delay estimation between two signals
         */
        DelayEstimator: DelayEstimatorConstructor;

        /**
         * Time stretching without pitch change
         */
        TimeStretcher: TimeStretcherConstructor;

        // ============================================
        // Audio Effects Classes
        // ============================================

        /**
         * Reverb effect for room simulation
         */
        Reverb: ReverbConstructor;

        /**
         * Delay/Echo effect
         */
        Delay: DelayConstructor;

        /**
         * Pitch shifter effect
         */
        PitchShifter: PitchShifterConstructor;

        /**
         * Chorus effect for thickening sound
         */
        Chorus: ChorusConstructor;

        /**
         * Flanger effect for modulation
         */
        Flanger: FlangerConstructor;

        // ============================================
        // Audio Utility Classes
        // ============================================

        /**
         * Audio sample buffer for storage and management
         */
        AudioBuffer: AudioBufferConstructor;

        /**
         * Audio level meter for signal monitoring
         */
        AudioLevel: AudioLevelConstructor;

        /**
         * Multi-channel audio mixer
         */
        AudioMixer: AudioMixerConstructor;

        /**
         * Jitter buffer for network audio
         */
        JitterBuffer: JitterBufferConstructor;

        // ============================================
        // Spatial Audio Classes
        // ============================================

        /**
         * 3D spatial audio renderer
         */
        SpatialRenderer: SpatialRendererConstructor;

        /**
         * Head-Related Transfer Function processor
         */
        HRTF: HRTFConstructor;

        // ============================================
        // Codec Classes
        // ============================================

        /**
         * G.711 μ-law/A-law codec
         */
        G711Codec: G711CodecConstructor;

        // ============================================
        // Watermarking Classes
        // ============================================

        /**
         * Audio watermark embedder
         */
        WatermarkEmbedder: WatermarkEmbedderConstructor;

        /**
         * Audio watermark detector
         */
        WatermarkDetector: WatermarkDetectorConstructor;

        // ============================================
        // Enums
        // ============================================

        EQPreset: typeof EQPreset;
        ReverbPreset: typeof ReverbPreset;

        // ============================================
        // Memory Management
        // ============================================

        _malloc(size: number): number;
        _free(ptr: number): void;
    }

    // ============================================
    // Constructor Interfaces
    // ============================================

    interface DenoiserConstructor {
        new(sampleRate: number, frameSize: number): Denoiser;
    }

    interface EchoCancellerConstructor {
        new(sampleRate: number, frameSize: number, filterLength: number): EchoCanceller;
    }

    interface AGCConstructor {
        new(sampleRate: number, targetLevel?: number): AGC;
    }

    interface ResamplerConstructor {
        new(inputRate: number, outputRate: number, quality?: number): Resampler;
    }

    interface VADConstructor {
        new(sampleRate: number, frameSize: number): VAD;
    }

    interface DTMFDetectorConstructor {
        new(sampleRate: number, frameSize: number): DTMFDetector;
    }

    interface DTMFGeneratorConstructor {
        new(sampleRate: number, toneDurationMs?: number, pauseDurationMs?: number): DTMFGenerator;
    }

    interface EqualizerConstructor {
        new(sampleRate: number, numBands?: number): Equalizer;
    }

    interface CompressorConstructor {
        new(sampleRate: number, thresholdDb?: number, ratio?: number,
            attackMs?: number, releaseMs?: number): Compressor;
    }

    interface LimiterConstructor {
        new(sampleRate: number, thresholdDb?: number): Limiter;
    }

    interface NoiseGateConstructor {
        new(sampleRate: number, thresholdDb?: number,
            attackMs?: number, releaseMs?: number): NoiseGate;
    }

    interface ComfortNoiseConstructor {
        new(sampleRate: number, frameSize: number, noiseLevelDb?: number): ComfortNoise;
    }

    interface DelayEstimatorConstructor {
        new(sampleRate: number, frameSize: number, maxDelayMs?: number): DelayEstimator;
    }

    interface TimeStretcherConstructor {
        new(sampleRate: number, channels?: number, initialRate?: number): TimeStretcher;
    }

    interface ReverbConstructor {
        new(sampleRate: number, roomSize?: number, damping?: number,
            wetLevel?: number, dryLevel?: number): Reverb;
    }

    interface DelayConstructor {
        new(sampleRate: number, delayMs?: number, feedback?: number, mix?: number): Delay;
    }

    interface PitchShifterConstructor {
        new(sampleRate: number, shiftSemitones?: number): PitchShifter;
    }

    interface ChorusConstructor {
        new(sampleRate: number, rate?: number, depth?: number, mix?: number): Chorus;
    }

    interface FlangerConstructor {
        new(sampleRate: number, rate?: number, depth?: number,
            feedback?: number, mix?: number): Flanger;
    }

    interface AudioBufferConstructor {
        new(capacitySamples: number, channels?: number): AudioBuffer;
    }

    interface AudioLevelConstructor {
        new(sampleRate: number, frameSize: number): AudioLevel;
    }

    interface AudioMixerConstructor {
        new(sampleRate: number, numInputs: number, frameSize: number): AudioMixer;
    }

    interface JitterBufferConstructor {
        new(sampleRate: number, frameSizeMs?: number,
            minDelayMs?: number, maxDelayMs?: number): JitterBuffer;
    }

    interface SpatialRendererConstructor {
        new(sampleRate: number, frameSize: number): SpatialRenderer;
    }

    interface HRTFConstructor {
        new(sampleRate: number): HRTF;
    }

    interface G711CodecConstructor {
        new(mode: string): G711Codec;
    }

    interface WatermarkEmbedderConstructor {
        /**
         * @param sampleRate Audio sample rate in Hz
         * @param strength Embedding strength: 0=LOW, 1=MEDIUM, 2=HIGH
         * @param seed Secret key for watermark (default: 12345)
         */
        new(sampleRate: number, strength?: number, seed?: number): WatermarkEmbedder;
    }

    interface WatermarkDetectorConstructor {
        /**
         * @param sampleRate Audio sample rate in Hz
         * @param seed Secret key (must match embedder's seed)
         */
        new(sampleRate: number, seed?: number): WatermarkDetector;
    }

    // ============================================
    // Class Interfaces
    // ============================================

    interface Denoiser {
        process(input: Int16Array): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface EchoCanceller {
        process(nearEnd: Int16Array, farEnd: Int16Array): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface AGC {
        process(input: Int16Array): Int16Array;
        getGain(): number;
        reset(): void;
        delete(): void;
    }

    interface Resampler {
        process(input: Int16Array): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface VAD {
        isSpeech(input: Int16Array): boolean;
        getProbability(): number;
        reset(): void;
        delete(): void;
    }

    interface DTMFDetector {
        process(input: Int16Array): string;
        getDigits(): string;
        clearDigits(): void;
        reset(): void;
        delete(): void;
    }

    interface DTMFGenerator {
        generateDigit(digit: string): Int16Array;
        generateSequence(digits: string): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface Equalizer {
        process(input: Int16Array): Int16Array;
        setBand(bandIndex: number, frequency: number, gainDb: number, q: number): void;
        setMasterGain(gainDb: number): void;
        applyPreset(preset: number): void;
        reset(): void;
        delete(): void;
    }

    interface Compressor {
        process(input: Int16Array): Int16Array;
        setThreshold(thresholdDb: number): void;
        setRatio(ratio: number): void;
        setTimes(attackMs: number, releaseMs: number): void;
        delete(): void;
    }

    interface Limiter {
        process(input: Int16Array): Int16Array;
        setThreshold(thresholdDb: number): void;
        delete(): void;
    }

    interface NoiseGate {
        process(input: Int16Array): Int16Array;
        setThreshold(thresholdDb: number): void;
        delete(): void;
    }

    interface ComfortNoise {
        analyze(input: Int16Array): void;
        generate(numSamples: number): Int16Array;
        setLevel(levelDb: number): void;
        getLevel(): number;
        reset(): void;
        delete(): void;
    }

    interface DelayEstimate {
        delaySamples: number;
        delayMs: number;
        confidence: number;
        valid: boolean;
    }

    interface DelayEstimator {
        estimate(reference: Int16Array, capture: Int16Array): DelayEstimate;
        getDelayMs(): number;
        isStable(): boolean;
        reset(): void;
        delete(): void;
    }

    interface TimeStretcher {
        process(input: Int16Array): Int16Array;
        setRate(rate: number): void;
        getRate(): number;
        reset(): void;
        delete(): void;
    }

    interface Reverb {
        process(input: Int16Array): Int16Array;
        setRoomSize(roomSize: number): void;
        setDamping(damping: number): void;
        setWetLevel(level: number): void;
        setDryLevel(level: number): void;
        setPreset(preset: number): void;
        reset(): void;
        delete(): void;
    }

    interface Delay {
        process(input: Int16Array): Int16Array;
        setDelayTime(delayMs: number): void;
        setFeedback(feedback: number): void;
        setMix(mix: number): void;
        reset(): void;
        delete(): void;
    }

    interface PitchShifter {
        process(input: Int16Array): Int16Array;
        setShift(semitones: number): void;
        getShift(): number;
        reset(): void;
        delete(): void;
    }

    interface Chorus {
        process(input: Int16Array): Int16Array;
        setRate(rate: number): void;
        setDepth(depth: number): void;
        setMix(mix: number): void;
        reset(): void;
        delete(): void;
    }

    interface Flanger {
        process(input: Int16Array): Int16Array;
        setRate(rate: number): void;
        setDepth(depth: number): void;
        setFeedback(feedback: number): void;
        setMix(mix: number): void;
        reset(): void;
        delete(): void;
    }

    interface AudioBuffer {
        write(input: Int16Array): void;
        read(numSamples: number): Int16Array;
        peek(numSamples: number): Int16Array;
        getAvailable(): number;
        getCapacity(): number;
        getFreeSpace(): number;
        clear(): void;
        delete(): void;
    }

    interface AudioLevel {
        process(input: Int16Array): void;
        getLevelDb(): number;
        getPeakDb(): number;
        getRmsDb(): number;
        isSilence(thresholdDb?: number): boolean;
        isClipping(): boolean;
        reset(): void;
        delete(): void;
    }

    interface AudioMixer {
        setInput(index: number, input: Int16Array): void;
        setInputGain(index: number, gain: number): void;
        setMasterGain(gain: number): void;
        mix(outputSamples: number): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface JitterStats {
        packetsReceived: number;
        packetsLost: number;
        packetsDiscarded: number;
        currentDelayMs: number;
        averageDelayMs: number;
        bufferLevel: number;
    }

    interface JitterBuffer {
        put(input: Int16Array, timestamp: number, sequence: number): void;
        get(numSamples: number): Int16Array;
        getStats(): JitterStats;
        getDelayMs(): number;
        reset(): void;
        delete(): void;
    }

    interface SpatialRenderer {
        setSourcePosition(x: number, y: number, z: number): void;
        setListenerPosition(x: number, y: number, z: number): void;
        setListenerOrientation(yaw: number, pitch: number, roll: number): void;
        process(input: Int16Array): Int16Array;
        setAttenuation(minDistance: number, maxDistance: number, rolloff: number): void;
        reset(): void;
        delete(): void;
    }

    interface HRTF {
        setAzimuth(azimuthDeg: number): void;
        setElevation(elevationDeg: number): void;
        setDistance(distance: number): void;
        process(input: Int16Array): Int16Array;
        reset(): void;
        delete(): void;
    }

    interface G711Codec {
        encode(input: Int16Array): Uint8Array;
        decode(input: Uint8Array): Int16Array;
        delete(): void;
    }

    interface WatermarkDetectResult {
        detected: boolean;
        confidence: number;
        correlation: number;
        snrDb: number;
        payload: number[];
        message: string;
    }

    interface WatermarkEmbedder {
        /** Set payload bytes to embed (max 256 bytes) */
        setPayload(data: number[]): boolean;
        /** Set payload from a string */
        setPayloadString(message: string): boolean;
        /** Embed watermark into audio samples */
        embed(audio: number[]): number[];
        /** Get number of bits embedded so far */
        getBitsEmbedded(): number;
        reset(): void;
        delete(): void;
    }

    interface WatermarkDetector {
        /** Process audio samples for watermark detection */
        detect(audio: number[]): WatermarkDetectResult;
        /** Get the last detection result */
        getResult(): WatermarkDetectResult;
        /** Check if detection is in progress */
        isDetecting(): boolean;
        reset(): void;
        delete(): void;
    }

    // ============================================
    // Enums
    // ============================================

    enum EQPreset {
        FLAT = 0,
        VOICE_ENHANCE = 1,
        TELEPHONE = 2,
        BASS_BOOST = 3,
        TREBLE_BOOST = 4,
        REDUCE_NOISE = 5,
        CLARITY = 6
    }

    enum ReverbPreset {
        SMALL_ROOM = 0,
        MEDIUM_ROOM = 1,
        LARGE_ROOM = 2,
        HALL = 3,
        CATHEDRAL = 4,
        PLATE = 5
    }
}

export = SonicKit;
