/**
 * @file platform_ios.m
 * @brief iOS-specific audio session management
 * 
 * 使用 AVAudioSession 管理 iOS 音频会话
 */

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#include "voice/platform.h"

/* ============================================
 * Global State
 * ============================================ */

static voice_interrupt_callback_t g_interrupt_cb = NULL;
static void *g_interrupt_cb_data = NULL;
static voice_route_change_callback_t g_route_change_cb = NULL;
static void *g_route_change_cb_data = NULL;

/* ============================================
 * Notification Observers
 * ============================================ */

@interface VoiceAudioSessionObserver : NSObject
@end

@implementation VoiceAudioSessionObserver

+ (instancetype)sharedInstance {
    static VoiceAudioSessionObserver *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[VoiceAudioSessionObserver alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        [self registerNotifications];
    }
    return self;
}

- (void)registerNotifications {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    
    [center addObserver:self
               selector:@selector(handleInterruption:)
                   name:AVAudioSessionInterruptionNotification
                 object:nil];
    
    [center addObserver:self
               selector:@selector(handleRouteChange:)
                   name:AVAudioSessionRouteChangeNotification
                 object:nil];
    
    [center addObserver:self
               selector:@selector(handleMediaServicesReset:)
                   name:AVAudioSessionMediaServicesWereResetNotification
                 object:nil];
}

- (void)handleInterruption:(NSNotification *)notification {
    if (!g_interrupt_cb) return;
    
    NSDictionary *info = notification.userInfo;
    AVAudioSessionInterruptionType type = 
        [info[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    
    voice_interrupt_type_t vtype = (type == AVAudioSessionInterruptionTypeBegan) ?
        VOICE_INTERRUPT_BEGAN : VOICE_INTERRUPT_ENDED;
    
    voice_interrupt_reason_t reason = VOICE_INTERRUPT_REASON_DEFAULT;
    bool shouldResume = false;
    
    if (type == AVAudioSessionInterruptionTypeEnded) {
        AVAudioSessionInterruptionOptions options = 
            [info[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
        shouldResume = (options & AVAudioSessionInterruptionOptionShouldResume) != 0;
    }
    
    if (@available(iOS 14.5, *)) {
        NSNumber *reasonNum = info[AVAudioSessionInterruptionReasonKey];
        if (reasonNum) {
            switch ([reasonNum unsignedIntegerValue]) {
                case AVAudioSessionInterruptionReasonDefault:
                    reason = VOICE_INTERRUPT_REASON_DEFAULT;
                    break;
                case AVAudioSessionInterruptionReasonAppWasSuspended:
                    reason = VOICE_INTERRUPT_REASON_APP_SUSPENDED;
                    break;
                case AVAudioSessionInterruptionReasonBuiltInMicMuted:
                    reason = VOICE_INTERRUPT_REASON_BUILT_IN_MIC_MUTED;
                    break;
            }
        }
    }
    
    g_interrupt_cb(vtype, reason, shouldResume, g_interrupt_cb_data);
}

- (void)handleRouteChange:(NSNotification *)notification {
    if (!g_route_change_cb) return;
    
    NSDictionary *info = notification.userInfo;
    AVAudioSessionRouteChangeReason reason = 
        [info[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
    
    voice_route_change_reason_t vreason;
    switch (reason) {
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            vreason = VOICE_ROUTE_CHANGE_NEW_DEVICE;
            break;
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
            vreason = VOICE_ROUTE_CHANGE_OLD_DEVICE_UNAVAILABLE;
            break;
        case AVAudioSessionRouteChangeReasonCategoryChange:
            vreason = VOICE_ROUTE_CHANGE_CATEGORY_CHANGE;
            break;
        case AVAudioSessionRouteChangeReasonOverride:
            vreason = VOICE_ROUTE_CHANGE_OVERRIDE;
            break;
        case AVAudioSessionRouteChangeReasonWakeFromSleep:
            vreason = VOICE_ROUTE_CHANGE_WAKE_FROM_SLEEP;
            break;
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            vreason = VOICE_ROUTE_CHANGE_NO_SUITABLE_ROUTE;
            break;
        case AVAudioSessionRouteChangeReasonRouteConfigurationChange:
            vreason = VOICE_ROUTE_CHANGE_CONFIG_CHANGE;
            break;
        default:
            vreason = VOICE_ROUTE_CHANGE_UNKNOWN;
            break;
    }
    
    voice_audio_route_t route = voice_session_get_current_route();
    g_route_change_cb(vreason, route, g_route_change_cb_data);
}

- (void)handleMediaServicesReset:(NSNotification *)notification {
    /* Re-configure audio session when media services are reset */
    voice_session_config_t config;
    voice_session_config_init(&config);
    voice_session_configure(&config);
}

@end

/* ============================================
 * C API Implementation
 * ============================================ */

static AVAudioSessionCategory categoryToAVCategory(voice_session_category_t cat) {
    switch (cat) {
        case VOICE_SESSION_CATEGORY_AMBIENT:
            return AVAudioSessionCategoryAmbient;
        case VOICE_SESSION_CATEGORY_SOLO_AMBIENT:
            return AVAudioSessionCategorySoloAmbient;
        case VOICE_SESSION_CATEGORY_PLAYBACK:
            return AVAudioSessionCategoryPlayback;
        case VOICE_SESSION_CATEGORY_RECORD:
            return AVAudioSessionCategoryRecord;
        case VOICE_SESSION_CATEGORY_PLAY_AND_RECORD:
            return AVAudioSessionCategoryPlayAndRecord;
        case VOICE_SESSION_CATEGORY_MULTI_ROUTE:
            return AVAudioSessionCategoryMultiRoute;
        default:
            return AVAudioSessionCategorySoloAmbient;
    }
}

static AVAudioSessionMode modeToAVMode(voice_session_mode_t mode) {
    switch (mode) {
        case VOICE_SESSION_MODE_DEFAULT:
            return AVAudioSessionModeDefault;
        case VOICE_SESSION_MODE_VOICE_CHAT:
            return AVAudioSessionModeVoiceChat;
        case VOICE_SESSION_MODE_VIDEO_CHAT:
            return AVAudioSessionModeVideoChat;
        case VOICE_SESSION_MODE_GAME_CHAT:
            return AVAudioSessionModeGameChat;
        case VOICE_SESSION_MODE_VOICE_PROMPT:
            return AVAudioSessionModeVoicePrompt;
        case VOICE_SESSION_MODE_MEASUREMENT:
            return AVAudioSessionModeMeasurement;
        default:
            return AVAudioSessionModeDefault;
    }
}

voice_error_t voice_session_configure(const voice_session_config_t *config) {
    if (!config) return VOICE_ERROR_NULL_POINTER;
    
    /* Ensure observer is initialized */
    [VoiceAudioSessionObserver sharedInstance];
    
    AVAudioSession *session = [AVAudioSession sharedInstance];
    NSError *error = nil;
    
    AVAudioSessionCategory category = categoryToAVCategory(config->category);
    AVAudioSessionMode mode = modeToAVMode(config->mode);
    
    AVAudioSessionCategoryOptions options = 0;
    if (config->options & VOICE_SESSION_OPTION_MIX_WITH_OTHERS) {
        options |= AVAudioSessionCategoryOptionMixWithOthers;
    }
    if (config->options & VOICE_SESSION_OPTION_DUCK_OTHERS) {
        options |= AVAudioSessionCategoryOptionDuckOthers;
    }
    if (config->options & VOICE_SESSION_OPTION_ALLOW_BLUETOOTH) {
        options |= AVAudioSessionCategoryOptionAllowBluetooth;
        options |= AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    }
    if (config->options & VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER) {
        options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
    }
    if (config->options & VOICE_SESSION_OPTION_INTERRUPT_SPOKEN) {
        options |= AVAudioSessionCategoryOptionInterruptSpokenAudioAndMixWithOthers;
    }
    
    if (![session setCategory:category mode:mode options:options error:&error]) {
        NSLog(@"Voice: Failed to set audio session category: %@", error);
        return VOICE_ERROR_DEVICE_OPEN;
    }
    
    /* Set preferred sample rate */
    if (config->preferred_sample_rate > 0) {
        [session setPreferredSampleRate:(double)config->preferred_sample_rate error:nil];
    }
    
    /* Set preferred buffer duration */
    if (config->preferred_io_buffer_duration > 0) {
        [session setPreferredIOBufferDuration:config->preferred_io_buffer_duration error:nil];
    }
    
    return VOICE_OK;
}

voice_error_t voice_session_activate(void) {
    NSError *error = nil;
    if (![[AVAudioSession sharedInstance] setActive:YES error:&error]) {
        NSLog(@"Voice: Failed to activate audio session: %@", error);
        return VOICE_ERROR_DEVICE_OPEN;
    }
    return VOICE_OK;
}

voice_error_t voice_session_deactivate(void) {
    NSError *error = nil;
    AVAudioSessionSetActiveOptions options = AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation;
    if (![[AVAudioSession sharedInstance] setActive:NO withOptions:options error:&error]) {
        NSLog(@"Voice: Failed to deactivate audio session: %@", error);
        return VOICE_ERROR_DEVICE_CLOSE;
    }
    return VOICE_OK;
}

voice_audio_route_t voice_session_get_current_route(void) {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    AVAudioSessionRouteDescription *route = session.currentRoute;
    
    for (AVAudioSessionPortDescription *output in route.outputs) {
        NSString *portType = output.portType;
        
        if ([portType isEqualToString:AVAudioSessionPortBuiltInSpeaker]) {
            return VOICE_AUDIO_ROUTE_BUILTIN_SPEAKER;
        } else if ([portType isEqualToString:AVAudioSessionPortBuiltInReceiver]) {
            return VOICE_AUDIO_ROUTE_BUILTIN_RECEIVER;
        } else if ([portType isEqualToString:AVAudioSessionPortHeadphones]) {
            return VOICE_AUDIO_ROUTE_HEADPHONES;
        } else if ([portType isEqualToString:AVAudioSessionPortBluetoothA2DP]) {
            return VOICE_AUDIO_ROUTE_BLUETOOTH_A2DP;
        } else if ([portType isEqualToString:AVAudioSessionPortBluetoothHFP]) {
            return VOICE_AUDIO_ROUTE_BLUETOOTH_HFP;
        } else if ([portType isEqualToString:AVAudioSessionPortBluetoothLE]) {
            return VOICE_AUDIO_ROUTE_BLUETOOTH_LE;
        } else if ([portType isEqualToString:AVAudioSessionPortUSBAudio]) {
            return VOICE_AUDIO_ROUTE_USB;
        } else if ([portType isEqualToString:AVAudioSessionPortHDMI]) {
            return VOICE_AUDIO_ROUTE_HDMI;
        } else if ([portType isEqualToString:AVAudioSessionPortLineOut]) {
            return VOICE_AUDIO_ROUTE_LINE_OUT;
        } else if ([portType isEqualToString:AVAudioSessionPortCarAudio]) {
            return VOICE_AUDIO_ROUTE_CAR_PLAY;
        } else if ([portType isEqualToString:AVAudioSessionPortAirPlay]) {
            return VOICE_AUDIO_ROUTE_AIR_PLAY;
        }
    }
    
    return VOICE_AUDIO_ROUTE_UNKNOWN;
}

voice_error_t voice_session_override_output(voice_audio_route_t route) {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    NSError *error = nil;
    
    AVAudioSessionPortOverride override;
    switch (route) {
        case VOICE_AUDIO_ROUTE_BUILTIN_SPEAKER:
            override = AVAudioSessionPortOverrideSpeaker;
            break;
        default:
            override = AVAudioSessionPortOverrideNone;
            break;
    }
    
    if (![session overrideOutputAudioPort:override error:&error]) {
        NSLog(@"Voice: Failed to override output port: %@", error);
        return VOICE_ERROR_DEVICE_OPEN;
    }
    
    return VOICE_OK;
}

void voice_session_set_interrupt_callback(
    voice_interrupt_callback_t callback,
    void *user_data) {
    g_interrupt_cb = callback;
    g_interrupt_cb_data = user_data;
    [VoiceAudioSessionObserver sharedInstance]; /* Ensure observer exists */
}

void voice_session_set_route_change_callback(
    voice_route_change_callback_t callback,
    void *user_data) {
    g_route_change_cb = callback;
    g_route_change_cb_data = user_data;
    [VoiceAudioSessionObserver sharedInstance];
}

bool voice_session_request_mic_permission(
    void (*callback)(bool granted, void *user_data),
    void *user_data) {
    
    AVAudioSession *session = [AVAudioSession sharedInstance];
    AVAudioSessionRecordPermission permission = session.recordPermission;
    
    if (permission == AVAudioSessionRecordPermissionGranted) {
        if (callback) callback(true, user_data);
        return true;
    }
    
    if (permission == AVAudioSessionRecordPermissionDenied) {
        if (callback) callback(false, user_data);
        return false;
    }
    
    /* Request permission */
    [session requestRecordPermission:^(BOOL granted) {
        if (callback) {
            dispatch_async(dispatch_get_main_queue(), ^{
                callback(granted, user_data);
            });
        }
    }];
    
    return false;
}

voice_permission_status_t voice_session_get_mic_permission(void) {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    AVAudioSessionRecordPermission permission = session.recordPermission;
    
    switch (permission) {
        case AVAudioSessionRecordPermissionGranted:
            return VOICE_PERMISSION_GRANTED;
        case AVAudioSessionRecordPermissionDenied:
            return VOICE_PERMISSION_DENIED;
        case AVAudioSessionRecordPermissionUndetermined:
        default:
            return VOICE_PERMISSION_UNKNOWN;
    }
}

voice_error_t voice_platform_enable_low_latency(bool enable) {
    /* iOS automatically optimizes for low latency in VoiceChat mode */
    return VOICE_OK;
}

voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frames_per_buffer) {
    
    AVAudioSession *session = [AVAudioSession sharedInstance];
    
    if (sample_rate) {
        *sample_rate = (uint32_t)session.sampleRate;
    }
    
    if (frames_per_buffer) {
        double duration = session.IOBufferDuration;
        *frames_per_buffer = (uint32_t)(session.sampleRate * duration);
    }
    
    return VOICE_OK;
}

voice_error_t voice_platform_set_bluetooth_sco(bool enable) {
    /* Bluetooth SCO is automatically handled by AVAudioSession on iOS */
    return VOICE_OK;
}

voice_error_t voice_platform_acquire_wake_lock(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled = YES;
    });
    return VOICE_OK;
}

voice_error_t voice_platform_release_wake_lock(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled = NO;
    });
    return VOICE_OK;
}

voice_error_t voice_platform_set_realtime_priority(void) {
    /* iOS handles audio thread priority automatically */
    return VOICE_OK;
}

#endif /* TARGET_OS_IPHONE */
#endif /* __APPLE__ */
