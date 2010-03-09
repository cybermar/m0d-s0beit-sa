/*

	PROJECT:		m0d_s0beit_sa
	LICENSE:		See LICENSE in the top level directory
	COPYRIGHT:		Copyright 2007, 2008, 2009, 2010 we_sux

	m0d_s0beit_sa is available from http://code.google.com/p/m0d-s0beit-sa/

	m0d_s0beit_sa is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	m0d_s0beit_sa is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with m0d_s0beit_sa.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "main.h"


// new function to help converting from vehicle_info->base to CEntitySAInterface
CEntitySAInterface* cheat_vehicle_GetCEntitySAInterface(vehicle_info *vinfo)
{
	return (CEntitySAInterface*)vinfo;
}
//*p_CEntitySAInterface = (CEntitySAInterface*)vinfo->base;



// new function to jump into vehicles without jacking (also for single player)
void vehicleJumper(int iVehicleID)
{
	traceLastFunc("vehicleJumper()");

	// can't touch this
	if (iVehicleID == VEHICLE_SELF) return;

	// get vehicle_info
	struct vehicle_info *pVehicle = vehicle_info_get(iVehicleID, 0);
	// check that the vehicle is legit
	if (isBadPtr_GTA_pVehicleInfo(pVehicle)) return;
	// if SAMP is loaded, check if the vehicle is streamed in
	if(g_SAMP != NULL)
	{
		int iVehicleSAMPID = getSAMPVehicleIDFromGTAVehicle(pVehicle);
		if (isBadPtr_SAMP_iVehicleID(iVehicleSAMPID)) return;
	}

	if (pVehicle->hitpoints == 0.0f) {
		cheat_state_text("Vehicle is destroyed");
		return;
	}
	if (cheat_state->actor.air_brake) {
		cheat_state_text("On foot airbrake must be disabled");
		return;
	}
	if (cheat_state->actor.stick) {
		cheat_state_text("On foot stick must be disabled");
		return;
	}
	if (!pVehicle->base.bUsesCollision) {
		cheat_state_text("Can't get in a vehicle that doesn't have collisions enabled.");
		return;
	}
	if (!pVehicle->base.bIsVisible)
	{
		cheat_state_text("Vehicle is not visible.");
		return;
	}
	struct actor_info *self = actor_info_get(ACTOR_SELF, 0);
	if(self != NULL && pVehicle->base.interior_id != self->base.interior_id)
	{
		cheat_state_text("Vehicle is in another interior.");
		return;
	}

	int iGTAVehicleID;
	iGTAVehicleID = ScriptCarId(pVehicle);

	if(pVehicle->passengers[0] == self)
		return;

	// put into first available seat
	if (pVehicle->passengers[0] == NULL) {
		ScriptCommand(&put_actor_in_car, ScriptActorId(self), iGTAVehicleID);
		ScriptCommand(&restore_camera_with_jumpcut);
		ScriptCommand(&set_camera_directly_behind);
		ScriptCommand(&restore_camera_with_jumpcut);
		return;
	}

	const int seat_count = gta_vehicle_get_by_id(pVehicle->base.model_alt_id)->passengers;
	if (seat_count > 0) {
		for (int seat = 1; seat <= seat_count; seat++) {
			if(pVehicle->passengers[seat] == NULL) {
				ScriptCommand(&put_actor_in_car_passenger, ScriptActorId(self), iGTAVehicleID, seat-1);
				ScriptCommand(&restore_camera_with_jumpcut);
				ScriptCommand(&set_camera_directly_behind);
				ScriptCommand(&restore_camera_with_jumpcut);
				return;
			}
		}
	}

	// no seats left, oh well
	cheat_state_text("No seats left to teleport into.");
	return;
}


void cheat_vehicle_teleport(struct vehicle_info *info, const float pos[3], int interior_id)
{
   if(info == NULL)
      return;

   float diff[3];
   float new_pos[3];
   struct vehicle_info *temp;

   vect3_vect3_sub(pos, &info->base.matrix[4*3], diff);

   for(temp = info; temp != NULL; temp = temp->trailer)
   {
      vect3_vect3_add(&temp->base.matrix[4*3], diff, new_pos);

      vehicle_detachables_teleport(temp, &temp->base.matrix[4*3], new_pos);
      vect3_copy(new_pos, &temp->base.matrix[4*3]);

      vect3_zero(temp->speed);
      vect3_zero(temp->spin);

      gta_interior_id_set(interior_id);
      temp->base.interior_id = (uint8_t)interior_id;

      if(!set.trailer_support)
         break;
   }
}


void cheat_handle_vehicle_unflip(struct vehicle_info *info, float time_diff)
{
	static float unflip_rotation;

	/* Unflip */
	if(KEY_PRESSED(set.key_unflip))
	{
		/* get the vehicle's yaw angle (z-axis) */
		unflip_rotation = atan2f(info->base.matrix[4*1+0], info->base.matrix[4*1+1]);
	}

	/* Rotate vehicle while the unflip key is held down */
	if(KEY_DOWN(set.key_unflip))
	{
		float a = unflip_rotation;
		float *m = info->base.matrix;
		float matrix[16] = {
			cosf(a),		-sinf(a),		0.0f,		0.0f,		// right
			sinf(a),		cosf(a),		0.0f,		0.0f,		// attitude
			0.0f,			0.0f,			1.0f,		0.0f,		// up
			m[4*3+0],		m[4*3+1],		m[4*3+2],	1.0f		// position
		};

		matrix_copy(matrix, info->base.matrix);
		vect3_zero(info->spin);
		unflip_rotation += time_diff * M_PI;
	}
}


void cheat_vehicle_air_brake_set(int enabled)
{
   cheat_state->vehicle.air_brake = enabled;
   cheat_handle_vehicle_air_brake(NULL, 0.0f);
}

void cheat_handle_vehicle_air_brake(struct vehicle_info *info, float time_diff)
{
   static float orig_matrix[16];
   static int orig_matrix_set;
   struct vehicle_info *temp;

   if(info == NULL)
   {
      orig_matrix_set = 0;
      return;
   }

   if(set.air_brake_toggle)
   {
	   if(KEY_PRESSED(set.key_air_brake_mod))
         cheat_state->vehicle.air_brake ^= 1;

      if(KEY_PRESSED(set.key_air_brake_mod2) && cheat_state->vehicle.air_brake)
         cheat_state->vehicle.air_brake_slowmo ^= 1;
   }
   else
   {
      if(KEY_PRESSED(set.key_air_brake_mod))
         cheat_state->vehicle.air_brake = 1;
      else if(KEY_RELEASED(set.key_air_brake_mod))
         cheat_state->vehicle.air_brake = 0;

      if(KEY_PRESSED(set.key_air_brake_mod2) && cheat_state->vehicle.air_brake)
         cheat_state->vehicle.air_brake_slowmo = 1;
      else if(KEY_RELEASED(set.key_air_brake_mod2) && cheat_state->vehicle.air_brake)
         cheat_state->vehicle.air_brake_slowmo = 0;
   }

   if(cheat_state->vehicle.air_brake && !orig_matrix_set)
   {
      matrix_copy(info->base.matrix, orig_matrix);
      orig_matrix_set = 1;
   }

   if(!cheat_state->vehicle.air_brake)
      orig_matrix_set = 0;

   if(cheat_state->vehicle.air_brake)
   {
      if(!KEY_DOWN(set.key_unflip))    /* allow the unflip rotation feature to work */
         matrix_copy(orig_matrix, info->base.matrix);

      /* XXX allow some movement */
      for(temp = info; temp != NULL; temp = temp->trailer)
      {
         /* prevent all kinds of movement */
         vect3_zero(temp->speed);
         vect3_zero(temp->spin);

         if(!set.trailer_support)
            break;
      }
   }

   if(cheat_state->vehicle.air_brake)
   {
      static uint32_t time_start;
      float *matrix = info->base.matrix;
      float d[4] = { 0.0f, 0.0f, 0.0f, time_diff * set.air_brake_speed };
      float xyvect[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; /* rotation vector */
      float zvect[4]  = { 0.0f, 0.0f, 0.0f, 0.0f }; /* rotation vector */
      float theta = set.air_brake_rot_speed * time_diff * M_PI * 2.0f;


      if(cheat_state->vehicle.air_brake_slowmo)
      {
         d[3] /= 10.0f;
         theta /= 2.0f;
      }

      if(KEY_DOWN(set.key_air_brake_forward))   d[0] += 1.0f;
      if(KEY_DOWN(set.key_air_brake_backward))  d[0] -= 1.0f;
      if(KEY_DOWN(set.key_air_brake_left))      d[1] += 1.0f;
      if(KEY_DOWN(set.key_air_brake_right))     d[1] -= 1.0f;
      if(KEY_DOWN(set.key_air_brake_up))        d[2] += 1.0f;
      if(KEY_DOWN(set.key_air_brake_down))      d[2] -= 1.0f;

      if(!near_zero(set.air_brake_accel_time))
      {
         if(!vect3_near_zero(d))
            time_start = (time_start == 0) ? time_get() : time_start;
         else
            time_start = 0;   /* no keys pressed */

         /* acceleration */
         if(time_start != 0)
         {
            float t = TIME_TO_FLOAT(time_get() - time_start);
            if(t < set.air_brake_accel_time)
               d[3] *= t / set.air_brake_accel_time;
         }
      }

      /* pitch (x-axis) */
      if(KEY_DOWN(set.key_air_brake_rot_pitch1))   xyvect[0] += 1.0f;
      if(KEY_DOWN(set.key_air_brake_rot_pitch2))   xyvect[0] -= 1.0f;
      /* roll (y-axis) */
      if(KEY_DOWN(set.key_air_brake_rot_roll1))    xyvect[1] += 1.0f;
      if(KEY_DOWN(set.key_air_brake_rot_roll2))    xyvect[1] -= 1.0f;
      /* yaw (z-axis) */
      if(KEY_DOWN(set.key_air_brake_rot_yaw1))     zvect[2] -= 1.0f;
      if(KEY_DOWN(set.key_air_brake_rot_yaw2))     zvect[2] += 1.0f;

      if(!vect3_near_zero(xyvect))
      {
         vect3_normalize(xyvect, xyvect);

         matrix_vect4_mult(matrix, xyvect, xyvect);
         matrix_vect3_rotate(matrix, xyvect, theta, matrix);
      }

      if(!vect3_near_zero(zvect))
      {
         vect3_normalize(zvect, zvect);

         matrix_vect3_rotate(matrix, zvect, theta, matrix);
      }

      switch(set.air_brake_behaviour)
      {
      case 0:
         matrix[4*1+0] += d[0] * d[3];
         matrix[4*1+1] += d[1] * d[3];
         matrix[4*1+2] += d[2] * d[3];
         break;

      case 1:  /* mixed 2d/3d mode */
         {
            /* convert the vehicle's 3d rotation vector to a 2d angle */
            float a = atan2f(matrix[4*1+0], matrix[4*1+1]);

            /* calculate new position */
            matrix[4*3+0] += (d[0] * sinf(a) - d[1] * cosf(a)) * d[3];
            matrix[4*3+1] += (d[0] * cosf(a) + d[1] * sinf(a)) * d[3];
            matrix[4*3+2] += d[2] * d[3];
         }
         break;

      case 2:  /* full 3d movement */
         if(!vect3_near_zero(d))
         {
            float vect[4] = { -d[1], d[0], d[2], 0.0f };  /* swap x & y + invert y */
            float out[4];

            /* out = matrix * norm(d) */
            vect3_normalize(vect, vect);
            matrix_vect4_mult(matrix, vect, out);

            matrix[4*3+0] += out[0] * d[3];
            matrix[4*3+1] += out[1] * d[3];
            matrix[4*3+2] += out[2] * d[3];
         }
         break;
      }

      matrix_copy(matrix, orig_matrix);
   }
}


void cheat_handle_vehicle_warp(struct vehicle_info *info, float time_diff)
{
   static struct vehicle_state state;
   static uint32_t warp_time = 0;
   static int warping = 0;


   if(warp_time != 0)
   {
      if(time_get() - warp_time > MSEC_TO_TIME(100))
      {
         warp_time = 0;
      }
      else
      {
         vehicle_state_restore(info, &state);
      }
   }

   /* warp factor: fuck you */
   if(KEY_PRESSED(set.key_warp_mod))
   {
      struct vehicle_info *temp;
      float dir[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
      float vect[4];

      if(set.warp_use_speed &&
         !vect3_near_zero(info->speed) &&
         vect3_length(info->speed) >= 0.01f)
      {
         /* use normalized speed vector */
         vect3_copy(info->speed, dir);
         vect3_normalize(dir, vect);
      }
      else
      {
         /* use vehicle direction matrix */
         matrix_vect4_mult(info->base.matrix, dir, vect);
         if(vect3_near_zero(vect))
            return;  /* shouldn't happen */
      }

      vehicle_state_store(info, &state);
      warp_time = 0;
      for(temp = info; temp != NULL; temp = temp->trailer)
      {
         vect3_mult(vect, set.warp_speed, temp->speed);

         if(!set.trailer_support)
            break;
      }
   }

   if(KEY_RELEASED(set.key_warp_mod))
   {
      struct vehicle_info *temp;

      warp_time = time_get();
      vehicle_state_restore(info, &state);
      for(temp = info; temp != NULL; temp = temp->trailer)
      {
         temp->collision_something = 0.0f;

         if(!set.trailer_support)
            break;
      }
   }
}


void cheat_handle_vehicle_hop(struct vehicle_info *info, float time_diff)
{
   struct vehicle_info *temp;

   if(KEY_DOWN(set.key_vehicle_hop))
      for(temp = info; temp != NULL; temp = temp->trailer)
      {
         temp->speed[2] = set.vehicle_hop_speed;

         if(!set.trailer_support)
            break;
      }
}


void cheat_handle_vehicle_brake(struct vehicle_info *info, float time_diff)
{
	float speed;
	struct vehicle_info *temp;

	if(KEY_DOWN(set.key_brake_mod))
	{
		for(temp = info; temp != NULL; temp = temp->trailer)
		{
			speed = vect3_length(temp->speed);
			vect3_normalize(temp->speed, temp->speed);
			speed -= time_diff * set.brake_mult;

			if(speed < 0.0f)
				speed = 0.0f;

			if(vect3_near_zero(temp->speed))
				vect3_zero(temp->speed);
			else
			{
				if(temp->vehicle_type == VEHICLE_TYPE_TRAIN){
					if(temp->m_fTrainSpeed <= 0.05f && temp->m_fTrainSpeed >= -0.05f)
						temp->m_fTrainSpeed = 0.0f;
					else if(temp->m_fTrainSpeed < 0.0f)
						temp->m_fTrainSpeed += time_diff * set.brake_mult;
					else
						temp->m_fTrainSpeed -= time_diff * set.brake_mult;
				}
				vect3_mult(temp->speed, speed, temp->speed);
			}

			if(!set.trailer_support)
				break;
		}
	}
}

void cheat_handle_vehicle_nitro(struct vehicle_info *info, float time_diff)
{
	static uint32_t timer;
	static float speed_off;
	float pre_speed[3];
	struct vehicle_info *temp;


	if(KEY_DOWN(set.key_warp_mod))
		vect3_copy(info->speed, pre_speed);

	if(KEY_PRESSED(set.key_nitro_mod))
	{
		speed_off = vect3_length(info->speed);
		timer = time_get();
	}

	/* "nitro" acceleration mod */
	if(KEY_DOWN(set.key_nitro_mod) && !vect3_near_zero(info->speed))
	{
		float etime = TIME_TO_FLOAT(time_get() - timer) / set.nitro_accel_time;
		float speed = set.nitro_high;

		if(!near_zero(set.nitro_accel_time))
			etime += 1.0f - (set.nitro_high - speed_off) / set.nitro_high;

		if(etime < 1.0f && !near_zero(set.nitro_accel_time))
			speed = set.nitro_high * etime;

		for(temp = info; temp != NULL; temp = temp->trailer)
		{
			if(!vect3_near_zero(temp->speed))
			{
				if(temp->vehicle_type == VEHICLE_TYPE_TRAIN){
					if(temp->m_fTrainSpeed < 0.0f && temp->m_fTrainSpeed > -set.nitro_high)
						temp->m_fTrainSpeed -= set.nitro_high * time_diff;
					else if(temp->m_fTrainSpeed < set.nitro_high)
						temp->m_fTrainSpeed += set.nitro_high * time_diff;
				}
				vect3_normalize(temp->speed, temp->speed);
				vect3_mult(temp->speed, speed, temp->speed);
				if(vect3_near_zero(temp->speed))
					vect3_zero(temp->speed);
			}

			if(!set.trailer_support)
				break;
		}
	}

	/* actual NOS of the game, toggle infinite NOS, w00t! */
	if(KEY_PRESSED(set.key_nitro))
	{
		if (!cheat_state->vehicle.infNOS_toggle_on)
		{
			cheat_state->vehicle.infNOS_toggle_on = true;
			patcher_install(&patch_vehicle_inf_NOS);
			vehicle_addUpgrade(info, 1010);
		}
		else
		{
			cheat_state->vehicle.infNOS_toggle_on = false;
			patcher_remove(&patch_vehicle_inf_NOS);
			GTAfunc_removeVehicleUpgrade(info, 1010);
		}
	}
}

void cheat_handle_vehicle_quick_turn_180(struct vehicle_info *info, float time_diff)
{
	if(KEY_PRESSED(set.key_quick_turn_180))
	{
		/* simply invert the X and Y axis.. */
		vect3_invert(&info->base.matrix[4*0], &info->base.matrix[4*0]);
		vect3_invert(&info->base.matrix[4*1], &info->base.matrix[4*1]);
		vect3_invert(info->speed, info->speed);
		if(info->vehicle_type == VEHICLE_TYPE_TRAIN)
		{
			for(struct vehicle_info *temp = info; temp != NULL; temp = temp->m_train_next_carriage)
			{
				if(!g_SAMP){
					temp->m_trainFlags.bDirection ^= 1;
					if(info->m_train_next_carriage == temp && info->base.model_alt_id == 538)
					{//avoid brown streak bug
						if(temp->m_fDistanceToNextCarriage == 16.5f)
							temp->m_fDistanceToNextCarriage = 20.8718f;
						else temp->m_fDistanceToNextCarriage = -16.5f;
					}
					temp->m_fDistanceToNextCarriage *= -1;
				}
				temp->m_fTrainSpeed = -info->m_fTrainSpeed;
			}
		}
	}
}

//MATRIX4X4
//	VECTOR right
//		float X [4*0+0]
//		float Y [4*0+1]
//		float Z [4*0+2] Roll
// DWORD  flags;
// VECTOR up;
//		float X [4*1+0]
//		float Y [4*1+1]
//		float Z [4*1+2] Pitch
// float  pad_u;
// VECTOR at;
//		float X [4*2+0]
//		float Y [4*2+1]
//		float Z [4*2+2]
// float  pad_a;
// VECTOR pos;
//		float X [4*3+0]
//		float Y [4*3+1]
//		float Z [4*3+2]
// float  pad_p;
void cheat_handle_vehicle_quick_turn_left(struct vehicle_info *vinfo, float time_diff)
{
	if(KEY_PRESSED(set.key_quick_turn_left))
	{
		// do new heading
		float *heading_matrix = vinfo->base.matrix;
		MATRIX4X4 *heading_matrix4x4 = vinfo->base.matrixStruct;
		// rotate on z axis
		CVector posUnder = cheat_vehicle_getPositionUnder(vinfo);
		float heading_vector_zrotate[3] = { posUnder.fX, posUnder.fY, posUnder.fZ };
		float heading_theta = M_PI / 2.0f;
		vect3_normalize(heading_vector_zrotate, heading_vector_zrotate);
		matrix_vect3_rotate(heading_matrix, heading_vector_zrotate, heading_theta, heading_matrix);
		// do new speed
		if (!vinfo->m_SpeedVec.IsNearZero())
		{
			vinfo->m_SpeedVec.CrossProduct(&posUnder);
		}
	}
}

void cheat_handle_vehicle_quick_turn_right(struct vehicle_info *vinfo, float time_diff)
{
	if(KEY_PRESSED(set.key_quick_turn_right))
	{
		// do new heading
		float *heading_matrix = vinfo->base.matrix;
		MATRIX4X4 *heading_matrix4x4 = vinfo->base.matrixStruct;
		// rotate on z axis
		CVector posUnder = cheat_vehicle_getPositionUnder(vinfo);
		posUnder = -posUnder;
		float heading_zvectrotate[4] = { posUnder.fX, posUnder.fY, posUnder.fZ };
		float heading_theta = M_PI / 2.0f;
		vect3_normalize(heading_zvectrotate, heading_zvectrotate);
		matrix_vect3_rotate(heading_matrix, heading_zvectrotate, heading_theta, heading_matrix);
		// do new speed
		if (!vinfo->m_SpeedVec.IsNearZero())
		{
			vinfo->m_SpeedVec.CrossProduct(&posUnder);
		}
	}
}


void cheat_handle_vehicle_protection(struct vehicle_info *info, float time_diff)
{
   static float last_spin[3];
   struct vehicle_info *temp;

   if(KEY_PRESSED(set.key_protection))
      cheat_state->vehicle.protection ^= 1;

   if(KEY_DOWN(set.key_warp_mod))
      return;

   if(cheat_state->vehicle.protection)
   {
      for(temp = info; temp != NULL; temp = temp->trailer)
      {
         if(!vect3_near_zero(temp->spin) && vect3_length(temp->spin) > set.protection_spin_cap)
         {
            vect3_normalize(temp->spin, temp->spin);
            vect3_mult(temp->spin, set.protection_spin_cap, temp->spin);
         }

         if(!vect3_near_zero(temp->speed) && vect3_length(temp->speed) > set.protection_speed_cap)
         {
            vect3_normalize(temp->speed, temp->speed);
            vect3_mult(temp->speed, set.protection_speed_cap, temp->speed);
         }

         if(temp->base.matrix[4*2+2] < 0.0f)
         {
            if(!vect3_near_zero(last_spin) && !vect3_near_zero(temp->spin) && vect3_length(temp->spin) < 0.10f)
            {
               vect3_normalize(temp->spin, temp->spin);
               vect3_mult(temp->spin, 0.10f, temp->spin);
            }
         }
         else if(temp == info) /* only copy the first vehicle's spin info */
         {
            vect3_copy(temp->spin, last_spin);
         }

         vehicle_prevent_below_height(temp, set.protection_min_height);

         if(!set.trailer_support)
            break;
      }
   }
}

void cheat_handle_vehicle_engine(struct vehicle_info *vehicle_info, float time_diff)
{
	if(vehicle_info == NULL) return;
	struct vehicle_info *veh_self = vehicle_info_get(VEHICLE_SELF, 0);
	if(veh_self == NULL) return;

	if(KEY_PRESSED(set.key_engine))
	{
		if(veh_self->m_nVehicleFlags.bEngineOn)
		{
			veh_self->m_nVehicleFlags.bEngineOn = 0;
			cheat_state->vehicle.is_engine_on = 0;
		} else {
			veh_self->m_nVehicleFlags.bEngineOn = 1;
			cheat_state->vehicle.is_engine_on = 1;
		}
	}
}

void cheat_handle_vehicle_brakedance(struct vehicle_info *vehicle_info, float time_diff)
{
	if(cheat_state->vehicle.air_brake) return;
	if(cheat_state->vehicle.stick) return;

	static float velpos, velneg;

	if(KEY_PRESSED(set.key_brkd_toggle))
	{
		cheat_state->vehicle.brkdance ^= 1;
	}

	if(cheat_state->vehicle.brkdance)
	{
		// we should probably actually be driving the vehicle
		struct actor_info *actor = actor_info_get(ACTOR_SELF, 0);
		if (actor == NULL) return; // we're not an actor? lulz
		if (actor->state != ACTOR_STATE_DRIVING) return; // we're not driving?
		if (actor->vehicle->passengers[1] == actor) return; // we're not passenger in an airplane?

		int iVehicleID = ScriptCarId(vehicle_info);

		float fTimeStep = *(float *)0xB7CB5C;

		velpos = set.brkdance_velocity * fTimeStep;
		velneg = -set.brkdance_velocity * fTimeStep;

		if(KEY_DOWN(set.key_brkd_forward))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, velneg, 0.0f, 0.0f);
		}
		else if(KEY_DOWN(set.key_brkd_backward))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, velpos, 0.0f, 0.0f);
		}
		else if(KEY_DOWN(set.key_brkd_right))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, 0.0f, velpos, 0.0f);
		}
		else if(KEY_DOWN(set.key_brkd_left))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, 0.0f, velneg, 0.0f);
		}
		else if(KEY_DOWN(set.key_brkd_rightward))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, 0.0f, 0.0f, velneg);
		}
		else if(KEY_DOWN(set.key_brkd_leftward))
		{
			ScriptCommand(&apply_momentum_in_direction_XYZ,      iVehicleID, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			ScriptCommand(&apply_rotory_pulse_about_an_axis_XYZ, iVehicleID, 0.0f, 0.0f, velpos);
		}
	}
}

void cheat_handle_blinking_carlights(struct vehicle_info *vinfo, float time_diff)
{
	traceLastFunc("cheat_handle_blinking_carlights()");

	if (KEY_PRESSED(set.key_blinking_car_lights))
	{
		// reset lights damage
		vinfo->lights_status_rear = false;
		vinfo->lights_status_frontLeft = false;
		vinfo->lights_status_frontRight = false;
		// increment lights state
		cheat_state->vehicle.blinking_carlights_state++;
		// set it back down to zero if over the max
		if (cheat_state->vehicle.blinking_carlights_state > 3) cheat_state->vehicle.blinking_carlights_state = 0;
		// reset turn state
		cheat_state->vehicle.blinking_carlights_turnstate = 0;
	}

	// return if lights state is 0/off
	if (!cheat_state->vehicle.blinking_carlights_state)
		return;

	// setup variables for next blinking lights modes
	struct actor_info *actor_self = actor_info_get(ACTOR_SELF, ACTOR_ALIVE);
	if (vinfo->passengers[0] != actor_self)
		return;
	int class_id = gta_vehicle_get_by_id(vinfo->base.model_alt_id)->class_id;
	if (vinfo->vehicle_type != VEHICLE_TYPE_CAR
		|| class_id == VEHICLE_CLASS_TRAILER
		|| class_id == VEHICLE_CLASS_AIRPLANE
		|| class_id == VEHICLE_CLASS_HELI) // lol at bikes
		return;
	// enables car lights any time of day, not synced
	ScriptCommand(&set_car_lights, ScriptCarId(vinfo), 2);


	// switch is faster than multiple if/else statements
	switch(cheat_state->vehicle.blinking_carlights_state)
	{
	case 1:
		// turn lights

		// blink rate limiter
		if ((GetTickCount()-250) < cheat_state->vehicle.blinking_carlights_lastblink)
			return;
		if (cheat_state->vehicle.blinking_carlights_turnstate == 0)
		{
			if (vinfo->steer_angles[0] <= -0.01f)
			{
				vinfo->lights_status_frontLeft = false;
				vinfo->lights_status_frontRight = true;
			}
			else if (vinfo->steer_angles[0] >= 0.01f)
			{
				vinfo->lights_status_frontLeft = true;
				vinfo->lights_status_frontRight = false;
			}
			cheat_state->vehicle.blinking_carlights_turnstate = 1;
		}
		else
		{
			vinfo->lights_status_frontLeft = false;
			vinfo->lights_status_frontRight = false;
			cheat_state->vehicle.blinking_carlights_turnstate = 0;
		}
		break;
	case 2:
		// police style lights

		// blink rate limiter
		if ((GetTickCount()-150) < cheat_state->vehicle.blinking_carlights_lastblink)
			return;

		// rear lights
		if (vinfo->lights_status_rear)
			vinfo->lights_status_rear = false;
		else
			vinfo->lights_status_rear = true;

		// front lights
		switch (cheat_state->vehicle.blinking_carlights_turnstate)
		{
		case 0:
			vinfo->lights_status_frontLeft = true;
			vinfo->lights_status_frontRight = false;
			break;
		case 1:
			vinfo->lights_status_frontLeft = false;
			vinfo->lights_status_frontRight = true;
			break;
		case 2:
			vinfo->lights_status_frontLeft = true;
			vinfo->lights_status_frontRight = true;
			break;
		case 3:
			vinfo->lights_status_frontLeft = false;
			vinfo->lights_status_frontRight = true;
			break;
		case 4:
			vinfo->lights_status_frontLeft = true;
			vinfo->lights_status_frontRight = false;
			break;
		case 5:
			vinfo->lights_status_frontLeft = true;
			vinfo->lights_status_frontRight = true;
			break;
		}

		// increment/reset turnstate
		cheat_state->vehicle.blinking_carlights_turnstate++;
		if (cheat_state->vehicle.blinking_carlights_turnstate > 5)
			cheat_state->vehicle.blinking_carlights_turnstate = 0;
		break;
	case 3:
		// stroboscope

		// blink rate limiter
		if ((GetTickCount()-25) < cheat_state->vehicle.blinking_carlights_lastblink)
			return;

		// rear lights
		if (vinfo->lights_status_rear)
		{
			vinfo->lights_status_rear = false;
			vinfo->lights_status_frontLeft = true;
			vinfo->lights_status_frontRight = true;
		}
		else
		{
			vinfo->lights_status_rear = true;
			vinfo->lights_status_frontLeft = false;
			vinfo->lights_status_frontRight = false;
		}
		break;
	}

	// reset counter
	cheat_state->vehicle.blinking_carlights_lastblink = GetTickCount();

	// not needed for the end of a void returning function
	//return;
}

void cheat_handle_vehicle_keepTrailer(struct vehicle_info *vinfo, float time_diff)
{
	traceLastFunc("cheat_handle_vehicle_keepTrailer()");

	if (KEY_PRESSED(set.key_keep_trailer))
		cheat_state->vehicle.keep_trailer_attached ^= 1;
	if (!cheat_state->vehicle.keep_trailer_attached)
		return;

	static struct vehicle_info *myveh_old;
	static struct vehicle_info *mytrailer_old;
	if (vinfo == myveh_old)
	{
		if (vinfo->trailer != NULL)
		{
			mytrailer_old = vinfo->trailer;
			return;
		}
		else if (mytrailer_old != NULL)
		{	
			DWORD car = ScriptCarId(mytrailer_old);
			if (car == NULL) return;

			if (vect3_dist(vinfo->base.coords,mytrailer_old->base.coords) <= 9.0f)
			{
				vinfo->trailer = mytrailer_old;
				ScriptCommand(&put_trailer_on_cab, car, ScriptCarId(vinfo));
			}
			else
			{
				mytrailer_old = NULL;
			}
		}
	}
	else if (vinfo->trailer != NULL)
	{
		myveh_old = vinfo;
		mytrailer_old = vinfo->trailer;
	}
	else
	{
		myveh_old = vinfo;
		mytrailer_old = NULL;
	}
}

void cheat_handle_fast_exit(struct vehicle_info *vehicle_info, float time_diff)
{
	if(KEY_PRESSED(set.key_fast_exit))
	{
		float *coord = (cheat_state->state == CHEAT_STATE_VEHICLE) ? cheat_state->vehicle.coords : cheat_state->actor.coords;
		struct actor_info *self = actor_info_get(ACTOR_SELF, ACTOR_ALIVE);
		ScriptCommand(&remove_actor_from_car_and_put_at,ScriptActorId(self),coord[0],coord[1] + 2.0f,coord[2]);
	}
}

void cheat_handle_repair_car(struct vehicle_info *vehicle_info, float time_diff)
{
	if(KEY_PRESSED(set.key_repair_car))
	{
		// get info
		struct vehicle_info *veh_self = vehicle_info_get(VEHICLE_SELF, 0);
		if(veh_self == NULL) return;
		struct actor_info *self = actor_info_get(ACTOR_SELF, 0);
		if(self == NULL) return;
		// make sure we are driving our own vehicle
		if(self->state == ACTOR_STATE_DRIVING
			&& self->vehicle->passengers[0] == self)
		{
			// fix the vehicle
			int iVehicleID = ScriptCarId(veh_self);
			ScriptCommand(&repair_car, iVehicleID);
		}
	}
}



































/*
CMatrix
	vRight = CVector ( 1.0f, 0.0f, 0.0f );
	vFront = CVector ( 0.0f, 1.0f, 0.0f );
	vUp    = CVector ( 0.0f, 0.0f, 1.0f );
	vPos   = CVector ( 0.0f, 0.0f, 0.0f );
MATRIX4X4
	VECTOR right;	// vRight
	VECTOR up;		// vFront
	VECTOR at;		// vUp
	VECTOR pos;		// vPos
*/

CVector cheat_vehicle_getPositionUnder(vehicle_info *vinfo)
{
	traceLastFunc("cheat_vehicle_getPositionUnder()");
	CVector offsetVector;
	float *matrix = vinfo->base.matrix;
	offsetVector.fX = 0 * matrix[0] + 0 * matrix[4] - 1 * matrix[8];
    offsetVector.fY = 0 * matrix[1] + 0 * matrix[5] - 1 * matrix[9];
    offsetVector.fZ = 0 * matrix[2] + 0 * matrix[6] - 1 * matrix[10];
	return offsetVector;
}

#ifdef M0D_DEV
/*
D3DXVECTOR3 vecGravColOrigin, vecGravColTarget, vecGravTargetNorm;
CVector temp_vecGravTargetNorm;
*/
#endif

void cheat_vehicle_setGravity(vehicle_info *vinfo, CVector pvecGravity)
{
	traceLastFunc("cheat_vehicle_setGravity()");

	// can't do yet, it'll crash cos we're not populating CVehicle into CPools yet
	//pGame->GetPools()->GetPedFromRef(1)->GetVehicle()->SetGravity(&pvecGravity);

	// set the d-dang gravity
	cheat_state->vehicle.gravityVector = pvecGravity;

	//5:08:18 PM lol cool
}

struct patch_set *patchBikeFalloff_set = NULL;
bool m_SpiderWheels_falloffFound = false;
bool m_SpiderWheels_falloffEnabled = false;
bool init_patchBikeFalloff(void)
{
	traceLastFunc("init_patchBikeFalloff()");

	if (!m_SpiderWheels_falloffFound)
	{
		int patchBikeFalloff_ID = GTAPatchIDFinder(0x004BA3B9);
		if (patchBikeFalloff_ID != -1)
		{
			patchBikeFalloff_set = &set.patch[patchBikeFalloff_ID];
			m_SpiderWheels_falloffFound = true;
		}
		else
		{
			Log("Couldn't init_patchBikeFalloff.");
			Log("You may fall off bikes while using SpiderWheels.");
			Log("Put the 'Anti bike fall off' patch back into your INI to fix this problem.");
		}
	}
	return m_SpiderWheels_falloffFound;
}

void cheat_handle_spiderWheels(struct vehicle_info *vinfo, float time_diff)
{
	traceLastFunc("cheat_handle_spiderWheels()");

	// 3:07:16 PM how you going

	if(KEY_PRESSED(set.key_spiderwheels))
	{
		// init variables used to toggle patch
		init_patchBikeFalloff();
		// toggle the d-dang spiderz
		cheat_state->vehicle.spiderWheels_on ^= 1;
	}

	if (cheat_state->vehicle.spiderWheels_on)
	{
		// update spider wheels
		CVector offsetVector = cheat_vehicle_getPositionUnder(vinfo);

		// setup variables
		CVector vecOrigin, vecTarget;
		CColPoint *pCollision = NULL;
		CEntitySAInterface *pCollisionEntity = NULL;
		int checkDistanceMeters = 20;

		// get CEntitySAInterface pointer
		CEntitySAInterface *p_CEntitySAInterface = cheat_vehicle_GetCEntitySAInterface(vinfo);

		// origin = our vehicle
		vecOrigin = p_CEntitySAInterface->Placeable.matrix->vPos;
		// target = vecOrigin + offsetVector * checkDistanceMeters
		vecTarget = offsetVector * checkDistanceMeters;
		vecTarget = vecTarget + vecOrigin;

		// check for collision
		bool bCollision = GTAfunc_ProcessLineOfSight(&vecOrigin, &vecTarget, &pCollision, &pCollisionEntity, 1, 0, 0, 1, 1, 0, 0, 0);

		if (bCollision)
		{
			// set altered gravity vector
			float fTimeStep = *(float *)0xB7CB5C;
			CVector colGravTemp = -pCollision->GetInterface()->Normal;
			CVector vehGravTemp = cheat_state->vehicle.gravityVector;
			CVector newRotVector;
			newRotVector = colGravTemp - vehGravTemp;
			newRotVector *= 0.05f * fTimeStep;
			offsetVector = vehGravTemp + newRotVector;
#ifdef M0D_DEV
/*
			vecGravColOrigin = D3DXVECTOR3(vecOrigin.fX, vecOrigin.fY, vecOrigin.fZ);
			vecGravColTarget.x = pCollision->GetInterface()->Position.fX;
			vecGravColTarget.y = pCollision->GetInterface()->Position.fY;
			vecGravColTarget.z = pCollision->GetInterface()->Position.fZ;
			temp_vecGravTargetNorm = vecOrigin + offsetVector * checkDistanceMeters;
			vecGravTargetNorm.x = temp_vecGravTargetNorm.fX;
			vecGravTargetNorm.y = temp_vecGravTargetNorm.fY;
			vecGravTargetNorm.z = temp_vecGravTargetNorm.fZ;
*/
#endif
			pCollision->Destroy();
		}
		else
		{
			// set normal gravity vector
			float fTimeStep = *(float *)0xB7CB5C;
			CVector colGravTemp;
			colGravTemp.fX = 0.0f;
			colGravTemp.fY = 0.0f;
			colGravTemp.fZ = -1.0f;
			CVector vehGravTemp = cheat_state->vehicle.gravityVector;
			CVector newRotVector;
			newRotVector = colGravTemp - vehGravTemp;
			newRotVector *= 0.05f * fTimeStep;
			offsetVector = vehGravTemp + newRotVector;
#ifdef M0D_DEV
/*
			vecGravColOrigin = D3DXVECTOR3(vecOrigin.fX, vecOrigin.fY, vecOrigin.fZ);
			vecGravColTarget = D3DXVECTOR3(vecTarget.fX, vecTarget.fY, vecTarget.fZ);
			temp_vecGravTargetNorm = vecOrigin + offsetVector * checkDistanceMeters;
			vecGravTargetNorm.x = temp_vecGravTargetNorm.fX;
			vecGravTargetNorm.y = temp_vecGravTargetNorm.fY;
			vecGravTargetNorm.z = temp_vecGravTargetNorm.fZ;
*/
#endif
		}

		// install anti bike falloff patch if needed
		if (!cheat_state->vehicle.spiderWheels_Enabled)
		{
			//ds
			if (m_SpiderWheels_falloffFound
				&& !m_SpiderWheels_falloffEnabled
				&& !patchBikeFalloff_set->installed)
			{
				patcher_install(patchBikeFalloff_set);
				m_SpiderWheels_falloffEnabled = true;
			}
			// set spider wheels enabled
			cheat_state->vehicle.spiderWheels_Enabled = true;
		}
		// set the gravity/camera
		cheat_vehicle_setGravity(vinfo, offsetVector);

	}
	else if (cheat_state->vehicle.spiderWheels_Enabled)
	{
		CVector offsetVector;
		// disable spider wheels with normal gravity vector
		offsetVector.fX = 0.0f;
		offsetVector.fY = 0.0f;
		offsetVector.fZ = -1.0f;

		// remove anti bike falloff patch if needed
		if ( m_SpiderWheels_falloffFound
			&& m_SpiderWheels_falloffEnabled)
		{
			if (patchBikeFalloff_set->installed || patchBikeFalloff_set->failed)
			{
				patcher_remove(patchBikeFalloff_set);
			}
			m_SpiderWheels_falloffEnabled = false;
		}

		// set the gravity/camera
		cheat_vehicle_setGravity(vinfo, offsetVector);
		// set spider wheels disabled
		cheat_state->vehicle.spiderWheels_Enabled = false;
	}
}
