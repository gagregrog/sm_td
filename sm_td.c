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
#include "sm_td.h"

/* ************************************* *
 *          DEBUG CONFIGURATION          *
 * ************************************* */

#ifdef SMTD_DEBUG_ENABLED
__attribute__((weak)) char *keycode_to_string_user(uint16_t keycode);

char *keycode_to_string(uint16_t keycode) {
    if (keycode_to_string_user) {
        char *result = keycode_to_string_user(keycode);
        if (result) {
            return result;
        }
    }

    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "KC_%d", keycode);
    return buffer;
}
#endif

/* ************************************* *
 *       USER TIMEOUT DEFINITIONS        *
 * ************************************* */

uint32_t get_smtd_timeout_default(smtd_timeout timeout) {
    switch (timeout) {
        case SMTD_TIMEOUT_TAP:
            return SMTD_GLOBAL_TAP_TERM;
        case SMTD_TIMEOUT_SEQUENCE:
            return SMTD_GLOBAL_SEQUENCE_TERM;
        case SMTD_TIMEOUT_FOLLOWING_TAP:
            return SMTD_GLOBAL_FOLLOWING_TAP_TERM;
        case SMTD_TIMEOUT_RELEASE:
            return SMTD_GLOBAL_RELEASE_TERM;
    }
    return 0;
}

uint32_t get_smtd_timeout_or_default(uint16_t keycode, smtd_timeout timeout) {
    if (get_smtd_timeout) {
        return get_smtd_timeout(keycode, timeout);
    }
    return get_smtd_timeout_default(timeout);
}

/* ************************************* *
 *    USER FEATURE FLAGS DEFINITIONS     *
 * ************************************* */

bool smtd_feature_enabled_default(smtd_feature feature) {
    switch (feature) {
        case SMTD_FEATURE_MODS_RECALL:
            return SMTD_GLOBAL_MODS_RECALL;
        case SMTD_FEATURE_AGGREGATE_TAPS:
            return SMTD_GLOBAL_AGGREGATE_TAPS;
    }
    return false;
}

bool smtd_feature_enabled_or_default(uint16_t keycode, smtd_feature feature) {
    if (smtd_feature_enabled) {
        return smtd_feature_enabled(keycode, feature);
    }
    return smtd_feature_enabled_default(feature);
}

/* ************************************* *
 *       USER ACTION DEFINITIONS         *
 * ************************************* */

#ifdef SMTD_DEBUG_ENABLED
char *smtd_action_to_string(smtd_action action) {
    switch (action) {
        case SMTD_ACTION_TOUCH:
            return "ACT_TOUCH";
        case SMTD_ACTION_TAP:
            return "ACT_TAP";
        case SMTD_ACTION_HOLD:
            return "ACT_HOLD";
        case SMTD_ACTION_RELEASE:
            return "ACT_RELEASE";
    }
    return "ACT_UNKNOWN";
}
#endif

/* ************************************* *
 *       USER STATES DEFINITIONS         *
 * ************************************* */

#ifdef SMTD_DEBUG_ENABLED
char *smtd_stage_to_string(smtd_stage stage) {
    switch (stage) {
        case SMTD_STAGE_NONE:
            return "STAGE_NONE";
        case SMTD_STAGE_TOUCH:
            return "STAGE_TOUCH";
        case SMTD_STAGE_SEQUENCE:
            return "STAGE_SEQUENCE";
        case SMTD_STAGE_FOLLOWING_TOUCH:
            return "STAGE_FOL_TOUCH";
        case SMTD_STAGE_HOLD:
            return "STAGE_HOLD";
        case SMTD_STAGE_RELEASE:
            return "STAGE_RELEASE";
    }
    return "STAGE_UNKNOWN";
}
#endif

/* ************************************* *
 *             LAYER UTILS               *
 * ************************************* */

static uint8_t return_layer     = RETURN_LAYER_NOT_SET;
static uint8_t return_layer_cnt = 0;

void avoid_unused_variable_on_compile(void *ptr) {
    // just touch them, so compiler won't throw "defined but not used" error
    // that variables are used in macros that user may not use
    if (return_layer == RETURN_LAYER_NOT_SET) return_layer = RETURN_LAYER_NOT_SET;
    if (return_layer_cnt == 0) return_layer_cnt = 0;
}

/* ************************************* *
 *      CORE LOGIC IMPLEMENTATION        *
 * ************************************* */

smtd_state smtd_active_states[10]  = {EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE, EMPTY_STATE};
uint8_t    smtd_active_states_size = 0;

void smtd_press_following_key(smtd_state *state, bool release) {
    state->freeze            = true;
    keyevent_t  event_press  = MAKE_KEYEVENT(state->following_key.row, state->following_key.col, true);
    keyrecord_t record_press = {.event = event_press};
#ifdef SMTD_DEBUG_ENABLED
    if (release) {
        printf("FOLLOWING_TAP(%s) by %s in %s\n", keycode_to_string(state->following_keycode), keycode_to_string(state->macro_keycode), smtd_stage_to_string(state->stage));
    } else {
        printf("FOLLOWING_PRESS(%s) by %s in %s\n", keycode_to_string(state->following_keycode), keycode_to_string(state->macro_keycode), smtd_stage_to_string(state->stage));
    }
#endif
    process_record(&record_press);
    if (release) {
        keyevent_t  event_release  = MAKE_KEYEVENT(state->following_key.row, state->following_key.col, false);
        keyrecord_t record_release = {.event = event_release};
        SMTD_SIMULTANEOUS_PRESSES_DELAY
        process_record(&record_release);
    }
    state->freeze = false;
}

uint32_t timeout_reset_seq(uint32_t trigger_time, void *cb_arg) {
    smtd_state *state   = (smtd_state *)cb_arg;
    state->sequence_len = 0;

    return 0;
}

uint32_t timeout_touch(uint32_t trigger_time, void *cb_arg) {
    smtd_state *state = (smtd_state *)cb_arg;
    smtd_next_stage(state, SMTD_STAGE_HOLD);
    return 0;
}

uint32_t timeout_sequence(uint32_t trigger_time, void *cb_arg) {
    smtd_state *state = (smtd_state *)cb_arg;

    if (smtd_feature_enabled_or_default(state->macro_keycode, SMTD_FEATURE_AGGREGATE_TAPS)) {
        DO_ACTION_TAP(state);
    }

    smtd_next_stage(state, SMTD_STAGE_NONE);
    return 0;
}

uint32_t timeout_following_touch(uint32_t trigger_time, void *cb_arg) {
    smtd_state *state = (smtd_state *)cb_arg;
    smtd_next_stage(state, SMTD_STAGE_HOLD);

    SMTD_SIMULTANEOUS_PRESSES_DELAY
    smtd_press_following_key(state, false);
    return 0;
}

uint32_t timeout_release(uint32_t trigger_time, void *cb_arg) {
    smtd_state *state = (smtd_state *)cb_arg;

    DO_ACTION_TAP(state);

    SMTD_SIMULTANEOUS_PRESSES_DELAY
    smtd_press_following_key(state, false);

    smtd_next_stage(state, SMTD_STAGE_NONE);
    return 0;
}

void smtd_next_stage(smtd_state *state, smtd_stage next_stage) {
#ifdef SMTD_DEBUG_ENABLED
    printf("STAGE by %s, %s -> %s\n", keycode_to_string(state->macro_keycode), smtd_stage_to_string(state->stage), smtd_stage_to_string(next_stage));
#endif

    deferred_token prev_token = state->timeout;
    state->timeout            = INVALID_DEFERRED_TOKEN;
    state->stage              = next_stage;

    switch (state->stage) {
        case SMTD_STAGE_NONE:
            for (uint8_t i = 0; i < smtd_active_states_size; i++) {
                if (&smtd_active_states[i] != state) continue;

                for (uint8_t j = i; j < smtd_active_states_size - 1; j++) {
                    smtd_active_states[j].macro_keycode      = smtd_active_states[j + 1].macro_keycode;
                    smtd_active_states[j].modes_before_touch = smtd_active_states[j + 1].modes_before_touch;
                    smtd_active_states[j].modes_with_touch   = smtd_active_states[j + 1].modes_with_touch;
                    smtd_active_states[j].sequence_len       = smtd_active_states[j + 1].sequence_len;
                    smtd_active_states[j].following_key      = smtd_active_states[j + 1].following_key;
                    smtd_active_states[j].following_keycode  = smtd_active_states[j + 1].following_keycode;
                    smtd_active_states[j].timeout            = smtd_active_states[j + 1].timeout;
                    smtd_active_states[j].stage              = smtd_active_states[j + 1].stage;
                    smtd_active_states[j].freeze             = smtd_active_states[j + 1].freeze;
                }

                smtd_active_states_size--;
                smtd_state *last_state         = &smtd_active_states[smtd_active_states_size];
                last_state->macro_keycode      = 0;
                last_state->modes_before_touch = 0;
                last_state->modes_with_touch   = 0;
                last_state->sequence_len       = 0;
                last_state->following_key      = MAKE_KEYPOS(0, 0);
                last_state->following_keycode  = 0;
                last_state->timeout            = INVALID_DEFERRED_TOKEN;
                last_state->stage              = SMTD_STAGE_NONE;
                last_state->freeze             = false;

                break;
            }
            break;

        case SMTD_STAGE_TOUCH:
            state->modes_before_touch = get_mods();
            SMTD_ACTION(SMTD_ACTION_TOUCH, state)
            state->modes_with_touch = get_mods() & ~state->modes_before_touch;
            state->timeout          = defer_exec(get_smtd_timeout_or_default(state->macro_keycode, SMTD_TIMEOUT_TAP), timeout_touch, state);
            break;

        case SMTD_STAGE_SEQUENCE:
            state->timeout = defer_exec(get_smtd_timeout_or_default(state->macro_keycode, SMTD_TIMEOUT_SEQUENCE), timeout_sequence, state);
            break;

        case SMTD_STAGE_HOLD:
            SMTD_ACTION(SMTD_ACTION_HOLD, state)
            break;

        case SMTD_STAGE_FOLLOWING_TOUCH:
            state->timeout = defer_exec(get_smtd_timeout_or_default(state->macro_keycode, SMTD_TIMEOUT_FOLLOWING_TAP), timeout_following_touch, state);
            break;

        case SMTD_STAGE_RELEASE:
            state->timeout = defer_exec(get_smtd_timeout_or_default(state->macro_keycode, SMTD_TIMEOUT_RELEASE), timeout_release, state);
            break;
    }

    // need to cancel after creating new timeout. There is a bug in QMK scheduling
    cancel_deferred_exec(prev_token);
}

bool process_smtd_state(uint16_t keycode, keyrecord_t *record, smtd_state *state) {
    if (state->freeze) {
        return true;
    }

    switch (state->stage) {
        case SMTD_STAGE_NONE:
            if (keycode == state->macro_keycode && record->event.pressed) {
                smtd_next_stage(state, SMTD_STAGE_TOUCH);
                return false;
            }
            return true;

        case SMTD_STAGE_TOUCH:
            if (keycode == state->macro_keycode && !record->event.pressed) {
                smtd_next_stage(state, SMTD_STAGE_SEQUENCE);

                if (!smtd_feature_enabled_or_default(state->macro_keycode, SMTD_FEATURE_AGGREGATE_TAPS)) {
                    DO_ACTION_TAP(state);
                }

                return false;
            }
            if (keycode != state->macro_keycode && record->event.pressed) {
                state->following_key     = record->event.key;
                state->following_keycode = keycode;
                smtd_next_stage(state, SMTD_STAGE_FOLLOWING_TOUCH);
                return false;
            }
            return true;

        case SMTD_STAGE_SEQUENCE:
            if (keycode == state->macro_keycode && record->event.pressed) {
                state->sequence_len++;
                smtd_next_stage(state, SMTD_STAGE_TOUCH);
                return false;
            }
            if (record->event.pressed) {
                if (smtd_feature_enabled_or_default(state->macro_keycode, SMTD_FEATURE_AGGREGATE_TAPS)) {
                    DO_ACTION_TAP(state);
                }

                smtd_next_stage(state, SMTD_STAGE_NONE);

                return true;
            }
            return true;

        case SMTD_STAGE_FOLLOWING_TOUCH:
            // At this stage, we have already pressed the macro key and the following key
            // none of them is assumed to be held yet

            if (keycode == state->macro_keycode && !record->event.pressed) {
                // Macro key is released, moving to the next stage
                smtd_next_stage(state, SMTD_STAGE_RELEASE);
                return false;
            }

            if (keycode != state->macro_keycode && (state->following_key.row == record->event.key.row && state->following_key.col == record->event.key.col) && !record->event.pressed) {
                // Following key is released. Now we definitely know that macro key is held
                // we need to execute hold the macro key and execute hold the following key
                // and then press move to next stage
                smtd_next_stage(state, SMTD_STAGE_HOLD);

                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_press_following_key(state, true);

                return false;
            }
            if (keycode != state->macro_keycode && !(state->following_key.row == record->event.key.row && state->following_key.col == record->event.key.col) && record->event.pressed) {
                // so, now we have 3rd key pressed
                // we assume this to be hold macro key, hold following key and press the 3rd key

                // need to put first key state into HOLD stage
                smtd_next_stage(state, SMTD_STAGE_HOLD);

                // then press and hold (without releasing) the following key
                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_press_following_key(state, false);

                // then rerun the 3rd key press
                // since we have just started hold stage, we need to simulate the press of the 3rd key again
                // because by holding first two keys we might have changed a layer, so current keycode might be not actual
                // if we don't do this, we might continue processing the wrong key
                SMTD_SIMULTANEOUS_PRESSES_DELAY
                state->freeze            = true;
                keyevent_t  event_press  = MAKE_KEYEVENT(record->event.key.row, record->event.key.col, true);
                keyrecord_t record_press = {.event = event_press};
                process_record(&record_press);
                state->freeze = false;

                // we have processed the 3rd key, so we intentionally return false to stop further processing
                return false;
            }
            return true;

        case SMTD_STAGE_HOLD:
            if (keycode == state->macro_keycode && !record->event.pressed) {
                SMTD_ACTION(SMTD_ACTION_RELEASE, state)

                smtd_next_stage(state, SMTD_STAGE_NONE);

                return false;
            }
            return true;

        case SMTD_STAGE_RELEASE:
            // At this stage we have just released the macro key and still holding the following key

            if (keycode == state->macro_keycode && record->event.pressed) {
                DO_ACTION_TAP(state);

                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_press_following_key(state, false);

                // todo need to go to NONE stage and from NONE jump to TOUCH stage
                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_next_stage(state, SMTD_STAGE_TOUCH);

                state->sequence_len = 0;

                return false;
            }
            if (keycode != state->macro_keycode && (state->following_key.row == record->event.key.row && state->following_key.col == record->event.key.col) && !record->event.pressed) {
                // Following key is released. Now we definitely know that macro key is held
                // we need to execute hold the macro key and execute tap the following key
                // then close the state

                SMTD_ACTION(SMTD_ACTION_HOLD, state)

                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_press_following_key(state, true);

                SMTD_SIMULTANEOUS_PRESSES_DELAY
                SMTD_ACTION(SMTD_ACTION_RELEASE, state)

                smtd_next_stage(state, SMTD_STAGE_NONE);

                return false;
            }
            if (keycode != state->macro_keycode && (state->following_key.row != record->event.key.row || state->following_key.col != record->event.key.col) && record->event.pressed) {
                // at this point we have already released the macro key and still holding the following key
                // and we get 3rd key pressed
                // we assume this to be tap macro key, press (w/o release) following key and press (w/o release) the 3rd key

                // so we need to tap the macro key first
                DO_ACTION_TAP(state)

                // then press and hold (without releasing) the following key
                SMTD_SIMULTANEOUS_PRESSES_DELAY
                smtd_press_following_key(state, false);

                // release current state, because the first key is already processed
                smtd_next_stage(state, SMTD_STAGE_NONE);

                // then rerun the 3rd key press
                // since we have just press following state, we need to simulate the press of the 3rd key again
                // because by pressing second key we might have changed a layer, so current keycode might be not actual
                // if we don't do this, we might continue processing the wrong key
                SMTD_SIMULTANEOUS_PRESSES_DELAY

                // we also don't need to freeze the state here, because we are already put in NONE stage
                keyevent_t  event_press  = MAKE_KEYEVENT(record->event.key.row, record->event.key.col, true);
                keyrecord_t record_press = {.event = event_press};
                process_record(&record_press);

                // we have processed the 3rd key, so we intentionally return false to stop further processing
                return false;
            }
            return true;
    }

    return true;
}

/* ************************************* *
 *      ENTRY POINT IMPLEMENTATION       *
 * ************************************* */

bool process_smtd(uint16_t keycode, keyrecord_t *record) {
#ifdef SMTD_DEBUG_ENABLED
    printf("\n>> GOT KEY %s %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED");
#endif

    // check if any active state may process an event
    for (uint8_t i = 0; i < smtd_active_states_size; i++) {
        smtd_state *state = &smtd_active_states[i];
        if (!process_smtd_state(keycode, record, state)) {
#ifdef SMTD_DEBUG_ENABLED
            printf("<< HANDLE KEY %s %s by %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED", keycode_to_string(state->macro_keycode));
#endif
            return false;
        }
    }

    // may be start a new state? A key must be just pressed
    if (!record->event.pressed) {
#ifdef SMTD_DEBUG_ENABLED
        printf("<< BYPASS KEY %s %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED");
#endif
        return true;
    }

    // check if the key is a macro key
    if (keycode <= SMTD_KEYCODES_BEGIN || SMTD_KEYCODES_END <= keycode) {
#ifdef SMTD_DEBUG_ENABLED
        printf("<< BYPASS KEY %s %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED");
#endif
        return true;
    }

    // check if the key is already handled
    for (uint8_t i = 0; i < smtd_active_states_size; i++) {
        if (smtd_active_states[i].macro_keycode == keycode) {
#ifdef SMTD_DEBUG_ENABLED
            printf("<< ALREADY HANDELED KEY %s %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED");
#endif
            return true;
        }
    }

    // create a new state and process the event
    smtd_state *state    = &smtd_active_states[smtd_active_states_size];
    state->macro_keycode = keycode;
    smtd_active_states_size++;

#ifdef SMTD_DEBUG_ENABLED
    printf("<< CREATE STATE %s %s\n", keycode_to_string(keycode), record->event.pressed ? "PRESSED" : "RELEASED");
#endif
    return process_smtd_state(keycode, record, state);
}
