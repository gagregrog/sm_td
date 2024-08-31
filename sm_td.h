/* Copyright 2024 Stanislav Markin (https://github.com/stasmarkin)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Version: 0.4.0
 * Date: 2024-03-07
 */
#pragma once

#include QMK_KEYBOARD_H
#ifdef QMK_USER_H
#    include QMK_USER_H
#endif
#include "deferred_exec.h"

#ifdef SMTD_DEBUG_ENABLED
#    include "print.h"
#endif

#ifdef SMTD_GLOBAL_SIMULTANEOUS_PRESSES_DELAY_MS
#    include "timer.h"
#endif

/* ************************************* *
 *         GLOBAL CONFIGURATION          *
 * ************************************* */

#ifndef SMTD_GLOBAL_SIMULTANEOUS_PRESSES_DELAY_MS
#    define SMTD_GLOBAL_SIMULTANEOUS_PRESSES_DELAY_MS 0
#endif

#if SMTD_GLOBAL_SIMULTANEOUS_PRESSES_DELAY_MS > 0
#    define SMTD_SIMULTANEOUS_PRESSES_DELAY wait_ms(SMTD_GLOBAL_SIMULTANEOUS_PRESSES_DELAY_MS);
#else
#    define SMTD_SIMULTANEOUS_PRESSES_DELAY
#endif

#ifndef SMTD_GLOBAL_TAP_TERM
#    define SMTD_GLOBAL_TAP_TERM TAPPING_TERM
#endif

#ifndef SMTD_GLOBAL_SEQUENCE_TERM
#    define SMTD_GLOBAL_SEQUENCE_TERM TAPPING_TERM / 2
#endif

#ifndef SMTD_GLOBAL_FOLLOWING_TAP_TERM
#    define SMTD_GLOBAL_FOLLOWING_TAP_TERM TAPPING_TERM
#endif

#ifndef SMTD_GLOBAL_RELEASE_TERM
#    define SMTD_GLOBAL_RELEASE_TERM TAPPING_TERM / 4
#endif

#ifndef SMTD_GLOBAL_MODS_RECALL
#    define SMTD_GLOBAL_MODS_RECALL true
#endif

#ifndef SMTD_GLOBAL_AGGREGATE_TAPS
#    define SMTD_GLOBAL_AGGREGATE_TAPS false
#endif

/* ************************************* *
 *          DEBUG CONFIGURATION          *
 * ************************************* */

#ifdef SMTD_DEBUG_ENABLED
__attribute__((weak)) char *keycode_to_string_user(uint16_t keycode);
char                       *keycode_to_string(uint16_t keycode)
#endif

    /* ************************************* *
     *       USER TIMEOUT DEFINITIONS        *
     * ************************************* */

    typedef enum {
        SMTD_TIMEOUT_TAP,
        SMTD_TIMEOUT_SEQUENCE,
        SMTD_TIMEOUT_FOLLOWING_TAP,
        SMTD_TIMEOUT_RELEASE,
    } smtd_timeout;

__attribute__((weak)) uint32_t get_smtd_timeout(uint16_t keycode, smtd_timeout timeout);

uint32_t get_smtd_timeout_default(smtd_timeout timeout);

uint32_t get_smtd_timeout_or_default(uint16_t keycode, smtd_timeout timeout);

/* ************************************* *
 *    USER FEATURE FLAGS DEFINITIONS     *
 * ************************************* */

typedef enum {
    SMTD_FEATURE_MODS_RECALL,
    SMTD_FEATURE_AGGREGATE_TAPS,
} smtd_feature;

__attribute__((weak)) bool smtd_feature_enabled(uint16_t keycode, smtd_feature feature);

bool smtd_feature_enabled_default(smtd_feature feature);

bool smtd_feature_enabled_or_default(uint16_t keycode, smtd_feature feature);

/* ************************************* *
 *       USER ACTION DEFINITIONS         *
 * ************************************* */

typedef enum {
    SMTD_ACTION_TOUCH,
    SMTD_ACTION_TAP,
    SMTD_ACTION_HOLD,
    SMTD_ACTION_RELEASE,
} smtd_action;

#ifdef SMTD_DEBUG_ENABLED
char *smtd_action_to_string(smtd_action action);
#endif

void on_smtd_action(uint16_t keycode, smtd_action action, uint8_t sequence_len);

#ifdef SMTD_DEBUG_ENABLED
#    define SMTD_ACTION(action, state)                                                                                                          \
        printf("%s by %s in %s\n", smtd_action_to_string(action), keycode_to_string(state->macro_keycode), smtd_stage_to_string(state->stage)); \
        on_smtd_action(state->macro_keycode, action, state->sequence_len);
#else
#    define SMTD_ACTION(action, state) on_smtd_action(state->macro_keycode, action, state->sequence_len);
#endif

/* ************************************* *
 *       USER STATES DEFINITIONS         *
 * ************************************* */

typedef enum {
    SMTD_STAGE_NONE,
    SMTD_STAGE_TOUCH,
    SMTD_STAGE_SEQUENCE,
    SMTD_STAGE_FOLLOWING_TOUCH,
    SMTD_STAGE_HOLD,
    SMTD_STAGE_RELEASE,
} smtd_stage;

#ifdef SMTD_DEBUG_ENABLED
char *smtd_stage_to_string(smtd_stage stage);
#endif

typedef struct {
    /** The keycode of the macro key */
    uint16_t macro_keycode;

    /** The mods before the touch action performed. Required for mod_recall feature */
    uint8_t modes_before_touch;

    /** Since touch can modify global mods, we need to save them separately to correctly restore a state before touch */
    uint8_t modes_with_touch;

    /** The length of the sequence of same key taps */
    uint8_t sequence_len;

    /** The position of key that was pressed after macro was pressed */
    keypos_t following_key;

    /** The keycode of the key that was pressed after macro was pressed */
    uint16_t following_keycode;

    /** The timeout of current stage */
    deferred_token timeout;

    /** The current stage of the state */
    smtd_stage stage;

    /** The flag that indicates that the state is frozen, so it won't handle any events */
    bool freeze;
} smtd_state;

#define EMPTY_STATE {.macro_keycode = 0, .modes_before_touch = 0, .modes_with_touch = 0, .sequence_len = 0, .following_key = MAKE_KEYPOS(0, 0), .following_keycode = 0, .timeout = INVALID_DEFERRED_TOKEN, .stage = SMTD_STAGE_NONE, .freeze = false}

/* ************************************* *
 *             LAYER UTILS               *
 * ************************************* */

#define RETURN_LAYER_NOT_SET 15

void avoid_unused_variable_on_compile(void *ptr);

#define LAYER_PUSH(layer)                              \
    return_layer_cnt++;                                \
    if (return_layer == RETURN_LAYER_NOT_SET) {        \
        return_layer = get_highest_layer(layer_state); \
    }                                                  \
    layer_move(layer);

#define LAYER_RESTORE()                          \
    if (return_layer_cnt > 0) {                  \
        return_layer_cnt--;                      \
        if (return_layer_cnt == 0) {             \
            layer_move(return_layer);            \
            return_layer = RETURN_LAYER_NOT_SET; \
        }                                        \
    }

/* ************************************* *
 *      CORE LOGIC IMPLEMENTATION        *
 * ************************************* */

#define DO_ACTION_TAP(state)                                                                                                            \
    uint8_t current_mods = get_mods();                                                                                                  \
    if (smtd_feature_enabled_or_default(state->macro_keycode, SMTD_FEATURE_MODS_RECALL) && state->modes_before_touch != current_mods) { \
        set_mods(state->modes_before_touch);                                                                                            \
        send_keyboard_report();                                                                                                         \
                                                                                                                                        \
        SMTD_SIMULTANEOUS_PRESSES_DELAY                                                                                                 \
        SMTD_ACTION(SMTD_ACTION_TAP, state)                                                                                             \
        uint8_t mods_diff = get_mods() ^ state->modes_before_touch;                                                                     \
                                                                                                                                        \
        SMTD_SIMULTANEOUS_PRESSES_DELAY                                                                                                 \
        set_mods(current_mods ^ mods_diff);                                                                                             \
        del_mods(state->modes_with_touch);                                                                                              \
        send_keyboard_report();                                                                                                         \
                                                                                                                                        \
        state->modes_before_touch = 0;                                                                                                  \
        state->modes_with_touch   = 0;                                                                                                  \
    } else {                                                                                                                            \
        SMTD_ACTION(SMTD_ACTION_TAP, state)                                                                                             \
    }

void smtd_press_following_key(smtd_state *state, bool release);

void smtd_next_stage(smtd_state *state, smtd_stage next_stage);

uint32_t timeout_reset_seq(uint32_t trigger_time, void *cb_arg);

uint32_t timeout_touch(uint32_t trigger_time, void *cb_arg);

uint32_t timeout_sequence(uint32_t trigger_time, void *cb_arg);

uint32_t timeout_following_touch(uint32_t trigger_time, void *cb_arg);

uint32_t timeout_release(uint32_t trigger_time, void *cb_arg);

void smtd_next_stage(smtd_state *state, smtd_stage next_stage);

bool process_smtd_state(uint16_t keycode, keyrecord_t *record, smtd_state *state);

/* ************************************* *
 *      ENTRY POINT IMPLEMENTATION       *
 * ************************************* */

bool process_smtd(uint16_t keycode, keyrecord_t *record);

/* ************************************* *
 *         CUSTOMIZATION MACROS          *
 * ************************************* */

#ifdef CAPS_WORD_ENABLE
#    define SMTD_TAP_16(use_cl, key) tap_code16(use_cl &&is_caps_word_on() ? LSFT(key) : key)
#    define SMTD_REGISTER_16(use_cl, key) register_code16(use_cl &&is_caps_word_on() ? LSFT(key) : key)
#    define SMTD_UNREGISTER_16(use_cl, key) unregister_code16(use_cl &&is_caps_word_on() ? LSFT(key) : key)
#else
#    define SMTD_TAP_16(use_cl, key) tap_code16(key)
#    define SMTD_REGISTER_16(use_cl, key) register_code16(key)
#    define SMTD_UNREGISTER_16(use_cl, key) unregister_code16(key)
#endif

#define SMTD_GET_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME
#define SMTD_MT(...) SMTD_GET_MACRO(__VA_ARGS__, SMTD_MT5, SMTD_MT4, SMTD_MT3)(__VA_ARGS__)
#define SMTD_MTE(...) SMTD_GET_MACRO(__VA_ARGS__, SMTD_MTE5, SMTD_MTE4, SMTD_MTE3)(__VA_ARGS__)
#define SMTD_LT(...) SMTD_GET_MACRO(__VA_ARGS__, SMTD_LT5, SMTD_LT4, SMTD_LT3)(__VA_ARGS__)

#define SMTD_MT3(macro_key, tap_key, mod) SMTD_MT4(macro_key, tap_key, mod, 1000)
#define SMTD_MTE3(macro_key, tap_key, mod) SMTD_MTE4(macro_key, tap_key, mod, 1000)
#define SMTD_LT3(macro_key, tap_key, layer) SMTD_LT4(macro_key, tap_key, layer, 1000)

#define SMTD_MT4(macro_key, tap_key, mod, threshold) SMTD_MT5(macro_key, tap_key, mod, threshold, true)
#define SMTD_MTE4(macro_key, tap_key, mod, threshold) SMTD_MTE5(macro_key, tap_key, mod, threshold, true)
#define SMTD_LT4(macro_key, tap_key, layer, threshold) SMTD_LT5(macro_key, tap_key, layer, threshold, true)

#define SMTD_MT5(macro_key, tap_key, mod, threshold, use_cl) \
    case macro_key: {                                        \
        switch (action) {                                    \
            case SMTD_ACTION_TOUCH:                          \
                break;                                       \
            case SMTD_ACTION_TAP:                            \
                SMTD_TAP_16(use_cl, tap_key);                \
                break;                                       \
            case SMTD_ACTION_HOLD:                           \
                if (tap_count < threshold) {                 \
                    register_mods(MOD_BIT(mod));             \
                } else {                                     \
                    SMTD_REGISTER_16(use_cl, tap_key);       \
                }                                            \
                break;                                       \
            case SMTD_ACTION_RELEASE:                        \
                if (tap_count < threshold) {                 \
                    unregister_mods(MOD_BIT(mod));           \
                } else {                                     \
                    SMTD_UNREGISTER_16(use_cl, tap_key);     \
                    send_keyboard_report();                  \
                }                                            \
                break;                                       \
        }                                                    \
        break;                                               \
    }

#define SMTD_MTE5(macro_key, tap_key, mod, threshold, use_cl) \
    case macro_key: {                                         \
        switch (action) {                                     \
            case SMTD_ACTION_TOUCH:                           \
                register_mods(MOD_BIT(mod));                  \
                break;                                        \
            case SMTD_ACTION_TAP:                             \
                unregister_mods(MOD_BIT(mod));                \
                SMTD_TAP_16(use_cl, tap_key);                 \
                break;                                        \
            case SMTD_ACTION_HOLD:                            \
                if (!(tap_count < threshold)) {               \
                    unregister_mods(MOD_BIT(mod));            \
                    SMTD_REGISTER_16(use_cl, tap_key);        \
                }                                             \
                break;                                        \
            case SMTD_ACTION_RELEASE:                         \
                if (tap_count < threshold) {                  \
                    unregister_mods(MOD_BIT(mod));            \
                    send_keyboard_report();                   \
                } else {                                      \
                    SMTD_UNREGISTER_16(use_cl, tap_key);      \
                }                                             \
                break;                                        \
        }                                                     \
        break;                                                \
    }

#define SMTD_LT5(macro_key, tap_key, layer, threshold, use_cl) \
    case macro_key: {                                          \
        switch (action) {                                      \
            case SMTD_ACTION_TOUCH:                            \
                break;                                         \
            case SMTD_ACTION_TAP:                              \
                SMTD_TAP_16(use_cl, tap_key);                  \
                break;                                         \
            case SMTD_ACTION_HOLD:                             \
                if (tap_count < threshold) {                   \
                    LAYER_PUSH(layer);                         \
                } else {                                       \
                    SMTD_REGISTER_16(use_cl, tap_key);         \
                }                                              \
                break;                                         \
            case SMTD_ACTION_RELEASE:                          \
                if (tap_count < threshold) {                   \
                    LAYER_RESTORE();                           \
                }                                              \
                SMTD_UNREGISTER_16(use_cl, tap_key);           \
                break;                                         \
        }                                                      \
        break;                                                 \
    }
