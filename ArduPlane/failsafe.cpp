#include "Plane.h"

/*
 *  failsafe support
 *  Andrew Tridgell, December 2011
 */

/*
 *  our failsafe strategy is to detect main loop lockup and switch to
 *  passing inputs straight from the RC inputs to RC outputs.
 */

/*
 *  this failsafe_check function is called from the core timer interrupt
 *  at 1kHz.
 */
void Plane::failsafe_check(void)
{
    static uint16_t last_mainLoop_count;
    static uint32_t last_timestamp;
    static bool in_failsafe;
    uint32_t tnow = micros();

    if (perf.mainLoop_count != last_mainLoop_count) {
        // the main loop is running, all is OK
        last_mainLoop_count = perf.mainLoop_count;
        last_timestamp = tnow;
        in_failsafe = false;
        return;
    }

    if (tnow - last_timestamp > 200000) {
        // we have gone at least 0.2 seconds since the main loop
        // ran. That means we're in trouble, or perhaps are in
        // an initialisation routine or log erase. Start passing RC
        // inputs through to outputs
        in_failsafe = true;
    }

    if (in_failsafe && tnow - last_timestamp > 20000) {
        last_timestamp = tnow;

        if (in_calibration) {
            // tell the failsafe system that we are calibrating
            // sensors, so don't trigger failsafe
            afs.heartbeat();
        }

        if (hal.rcin->num_channels() < 5) {
            // we don't have any RC input to pass through
            return;
        }

        // pass RC inputs to outputs every 20ms
        hal.rcin->clear_overrides();
        channel_roll->set_radio_out(channel_roll->read());
        channel_pitch->set_radio_out(channel_pitch->read());
        if (hal.util->get_soft_armed()) {
            channel_throttle->set_radio_out(channel_throttle->read());
        } else {
            channel_throttle->set_servo_out(0);
            channel_throttle->calc_pwm();
        }
        channel_rudder->set_radio_out(channel_rudder->read());

        int16_t roll = channel_roll->pwm_to_angle_dz(0);
        int16_t pitch = channel_pitch->pwm_to_angle_dz(0);
        int16_t rudder = channel_rudder->pwm_to_angle_dz(0);

        // setup secondary output channels that don't have
        // corresponding input channels
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_aileron, roll);
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_elevator, pitch);
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_rudder, rudder);
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_steering, rudder);

        // this is to allow the failsafe module to deliberately crash 
        // the plane. Only used in extreme circumstances to meet the
        // OBC rules
        if (afs.should_crash_vehicle()) {
            afs.terminate_vehicle();
            return;
        }

        // setup secondary output channels that do have
        // corresponding input channels
        RC_Channel_aux::copy_radio_in_out(RC_Channel_aux::k_manual, true);
        RC_Channel_aux::copy_radio_in_out(RC_Channel_aux::k_aileron_with_input, true);
        RC_Channel_aux::copy_radio_in_out(RC_Channel_aux::k_elevator_with_input, true);
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_flap, 0);
        RC_Channel_aux::set_servo_out_for(RC_Channel_aux::k_flap_auto, 0);

        servos_output();
        
        // setup flaperons
        flaperon_update(0);
    }
}
