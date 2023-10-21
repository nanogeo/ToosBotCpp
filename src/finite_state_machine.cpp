#pragma once
#include "finish_state_machine.h"
#include "locations.h"
#include "utility.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <math.h>


#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit_filters.h"
#include "sc2lib/sc2_lib.h"

#include "TossBot.h"

namespace sc2 {

#pragma region State

	void State::TickState()
	{
		std::cout << "TickState called on base State class";
	}

	void State::EnterState()
	{
		std::cout << "EnterState called on base State class";
	}

	void State::ExitState()
	{
		std::cout << "ExitState called on base State class";
	}

	State* State::TestTransitions()
	{
		std::cout << "TestTransitions called on base State class";
		return NULL;
	}

	std::string State::toString()
	{
		return "State";
	}

#pragma endregion

#pragma region OracleDefendLocation

	std::string OracleDefendLocation::toString()
	{
		return "Oracle Defend Location";
	}

	void OracleDefendLocation::TickState()
	{
		if (agent->Observation()->GetUnits(Unit::Alliance::Enemy).size() > 0 && Utility::DistanceToClosest(agent->Observation()->GetUnits(Unit::Alliance::Enemy), denfensive_position) < 10)
		{
			for (const auto &oracle : state_machine->oracles)
			{
				float now = agent->Observation()->GetGameLoop() / 22.4;
				bool weapon_ready = now - state_machine->time_last_attacked[oracle] > .61;
				bool beam_active = state_machine->is_beam_active[oracle];
				bool beam_activatable = false;

				if (!beam_active && oracle->energy >= 40)
					beam_activatable = true;

				const Unit* closest_unit = Utility::ClosestTo(agent->Observation()->GetUnits(Unit::Alliance::Enemy), oracle->pos);
				if (beam_active)
				{
					if (Distance2D(oracle->pos, closest_unit->pos) > 4)
					{
						float dist = Distance2D(oracle->pos, closest_unit->pos);
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, Utility::PointBetween(oracle->pos, closest_unit->pos, dist + 1), false);
					}
					else if (weapon_ready)
					{
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::ATTACK_ATTACK, closest_unit, false);
						state_machine->time_last_attacked[oracle] = now;
						state_machine->has_attacked[oracle] = false;
						agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(255, 255, 0));
					}
					else if (state_machine->has_attacked[oracle])
					{
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, closest_unit->pos, false);
						agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 0, 255));
					}
					else
					{
						agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(255, 0, 0));
					}
				}
				else if (beam_activatable)
				{
					if (Distance2D(oracle->pos, closest_unit->pos) < 3)
					{
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMON, false);
						state_machine->is_beam_active[oracle] = true;
					}
					else
					{
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, closest_unit->pos, false);
					}
				}
				else
				{
					agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, closest_unit->pos, false);
					agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 255, 0));
				}
				agent->Debug()->DebugTextOut(std::to_string(now - state_machine->time_last_attacked[oracle]), Point2D(.7, .7), Color(0, 255, 255), 20);
				agent->Debug()->DebugTextOut(std::to_string(agent->Observation()->GetGameLoop()), Point2D(.7, .75), Color(0, 255, 255), 20);

			}
		}
		else
		{
			for (const auto &oracle : state_machine->oracles)
			{
				if (state_machine->is_beam_active[oracle])
				{
					agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
					state_machine->is_beam_active[oracle] = false;
				}

				if (Distance2D(oracle->pos, denfensive_position) > 1)
					agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, denfensive_position);
			}
		}


	}

	void OracleDefendLocation::EnterState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::GENERAL_MOVE, denfensive_position);
		}
		std::function<void(const Unit*, float, float)> onUnitDamaged = [=](const Unit* unit, float health, float shields) {
			this->OnUnitDamagedListener(unit, health, shields);
		};
		agent->AddListenerToOnUnitDamagedEvent(event_id, onUnitDamaged);

		std::function<void(const Unit*)> onUnitDestroyed = [=](const Unit* unit) {
			this->OnUnitDestroyedListener(unit);
		};
		agent->AddListenerToOnUnitDestroyedEvent(event_id, onUnitDestroyed);
	}

	void OracleDefendLocation::ExitState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			// remove event onUnitDamaged
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
		}
		agent->RemoveListenerToOnUnitDamagedEvent(event_id);
		agent->RemoveListenerToOnUnitDestroyedEvent(event_id);
		return;
	}

	State* OracleDefendLocation::TestTransitions()
	{
		if (state_machine->oracles.size() > 1) //> 1
		{
			if (true) //(agent->Observation()->GetGameLoop() % 2 == 0)
			{
				state_machine->harass_direction = true;
				state_machine->harass_index = 0;
				return new OracleHarassGroupUp(agent, state_machine, agent->locations->oracle_path.entrance_point);
			}
			else
			{
				state_machine->harass_direction = false;
				state_machine->harass_index = agent->locations->oracle_path.entrance_points.size() - 1;
				return new OracleHarassGroupUp(agent, state_machine, agent->locations->oracle_path.exit_point);
			}
		}
		return NULL;
	}

	void OracleDefendLocation::OnUnitDamagedListener(const Unit* unit, float health, float shields)
	{
		std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " took " << std::to_string(health) << " damage\n";
		for (const auto &oracle : state_machine->oracles)
		{
			if (oracle->engaged_target_tag == unit->tag)
			{
				std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " took " << std::to_string(health) << " damage from orale\n";
				state_machine->has_attacked[oracle] = true;
			}
		}
	}

	void OracleDefendLocation::OnUnitDestroyedListener(const Unit* unit)
	{
		std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed\n";
		for (const auto &oracle : state_machine->oracles)
		{
			if (oracle->engaged_target_tag == unit->tag)
			{
				std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed by orale\n";
				state_machine->has_attacked[oracle] = true;
			}
		}
	}


#pragma endregion


#pragma region OracleDefendArmyGroup

	std::string OracleDefendArmyGroup::toString()
	{
		return "Oracle Defend ArmyGroup";
	}

	void OracleDefendArmyGroup::TickState()
	{
		Point2D center = state_machine->attached_army_group->attack_path[state_machine->attached_army_group->current_attack_index - 2];
		if (state_machine->attached_army_group->all_units.size() > 0)
			center = Utility::MedianCenter(state_machine->attached_army_group->all_units);

		Units enemy_units = agent->Observation()->GetUnits(IsUnits(Utility::GetBurrowedUnitTypes()));

		bool revelation_cast = false;
		for (const auto &oracle : state_machine->oracles)
		{
			if (state_machine->casting[oracle])
			{
				revelation_cast = true;
				break;
			}
		}
		// revelate when units are burrowing
		if (!revelation_cast)
		{
			const Unit* unit_to_revelate = NULL;
			for (const auto &unit : enemy_units)
			{
				if (Utility::DistanceToClosest(state_machine->oracles, unit->pos) <= 9)
				{
					if (std::find(unit->buffs.begin(), unit->buffs.end(), BUFF_ID::ORACLEREVELATION) == unit->buffs.end())
					{
						unit_to_revelate = unit;
						break;
					}
				}
			}
			if (unit_to_revelate != NULL)
			{
				const Unit* highest_over_75 = NULL;
				const Unit* lowest_over_25 = NULL;
				for (const auto &oracle : state_machine->oracles)
				{
					if (oracle->energy > 75)
					{
						if (highest_over_75 == NULL || highest_over_75->energy < oracle->energy)
							highest_over_75 = oracle;
					}
					else if (oracle->energy > 25)
					{
						if (lowest_over_25 == NULL || lowest_over_25->energy > oracle->energy)
							lowest_over_25 = oracle;
					}
				}
				if (highest_over_75 != NULL)
				{
					agent->Actions()->UnitCommand(highest_over_75, ABILITY_ID::EFFECT_ORACLEREVELATION, unit_to_revelate->pos);
					state_machine->casting[highest_over_75] = true;
					state_machine->casting_energy[highest_over_75] = highest_over_75->energy;
					agent->Debug()->DebugSphereOut(highest_over_75->pos, 2, Color(255, 0, 0));

				}
				else if (lowest_over_25 != NULL)
				{
					agent->Actions()->UnitCommand(lowest_over_25, ABILITY_ID::EFFECT_ORACLEREVELATION, unit_to_revelate->pos);
					state_machine->casting[lowest_over_25] = true;
					state_machine->casting_energy[lowest_over_25] = lowest_over_25->energy;
					agent->Debug()->DebugSphereOut(lowest_over_25->pos, 2, Color(255, 0, 0));
				}
			}
		}


		Units enemy_lings = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::ZERG_ZERGLING));
		int num_close_lings = 0;
		for (const auto &ling : enemy_lings)
		{
			if (Utility::DistanceToClosest(state_machine->attached_army_group->all_units, ling->pos) < 4)
				num_close_lings++;
		}
		if (num_close_lings > 4)
		{
			int num_stalkers_with_blink = 0;
			float now = agent->Observation()->GetGameLoop() / 22.4;
			for (const auto &last_blink_time : state_machine->attached_army_group->last_time_blinked)
			{
				if (now - last_blink_time.second > 7)
					num_stalkers_with_blink++;
			}
			float percent_stalkers_with_blink = 1;
			if (state_machine->attached_army_group->last_time_blinked.size() > 0)
				percent_stalkers_with_blink = static_cast<float>(num_stalkers_with_blink) / static_cast<float>(state_machine->attached_army_group->last_time_blinked.size());

			int num_oracles_needed = 0;

			if (percent_stalkers_with_blink < .25)
				num_oracles_needed = 3;
			else if (percent_stalkers_with_blink < .5)
				num_oracles_needed = 2;
			else if (percent_stalkers_with_blink < .75)
				num_oracles_needed = 1;

			if (num_close_lings > 30)
				num_oracles_needed += 3;
			else if (num_close_lings > 20)
				num_oracles_needed += 2;
			else if (num_close_lings > 10)
				num_oracles_needed += 1;
			

			num_oracles_needed = std::min(num_oracles_needed, 3);

			int num_oracles_active = 0;
			for (const auto &beam_active : state_machine->is_beam_active)
			{
				if (beam_active.second)
					num_oracles_active++;
			}
			/*
			agent->Debug()->DebugTextOut(std::to_string(num_close_lings), Point2D(.2, .18), Color(0, 255, 0), 20);
			agent->Debug()->DebugTextOut(std::to_string(num_oracles_active), Point2D(.2, .2), Color(0, 255, 255), 20);
			agent->Debug()->DebugTextOut(std::to_string(num_oracles_needed), Point2D(.2, .22), Color(0, 255, 255), 20);
			agent->Debug()->DebugTextOut(std::to_string(percent_stalkers_with_blink), Point2D(.2, .24), Color(0, 255, 255), 20);
			agent->Debug()->DebugTextOut(std::to_string(num_stalkers_with_blink), Point2D(.2, .26), Color(0, 255, 255), 20);
			agent->Debug()->DebugTextOut(std::to_string(state_machine->attached_army_group->blink_ready.size()), Point2D(.2, .28), Color(0, 255, 255), 20);
			
			for (int i = 0; i < state_machine->oracles.size(); i++)
			{
				if (state_machine->is_beam_active[i])
					agent->Debug()->DebugTextOut(std::to_string(state_machine->oracles[i]->energy), Point2D(.2, .3 + .02 * i), Color(0, 255, 0), 20);
				else
					agent->Debug()->DebugTextOut(std::to_string(state_machine->oracles[i]->energy), Point2D(.2, .3 + .02 * i), Color(255, 0, 255), 20);

			}*/

			if (num_oracles_active > num_oracles_needed) // deactivate oracles
			{
				Units oracles = Units(state_machine->oracles);
				std::sort(oracles.begin(), oracles.end(), [](const Unit* &a, const Unit* &b) -> bool
				{
					return a->energy > b->energy;
				});
				for (const auto &oracle : oracles)
				{
					if (num_oracles_active == num_oracles_needed)
						break;
					if (oracle->energy > 10 && Utility::DistanceToClosest(enemy_lings, oracle->pos) > 5)
					{
						if (state_machine->is_beam_active.count(oracle) && state_machine->is_beam_active[oracle] == true)
						{
							state_machine->is_beam_active[oracle] = false;
							agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
							num_oracles_active--;
						}
					}
				}
			}
			else if (num_oracles_active < num_oracles_needed) // activate more oracles
			{
				Units oracles = Units(state_machine->oracles);
				std::sort(oracles.begin(), oracles.end(), [](const Unit* &a, const Unit* &b) -> bool
				{
					return a->energy < b->energy;
				});
				for (const auto &oracle : oracles)
				{
					if (num_oracles_active == num_oracles_needed)
						break;
					if (oracle->energy > 40)
					{
						if (state_machine->is_beam_active.count(oracle) && state_machine->is_beam_active[oracle] == false)
						{
							state_machine->is_beam_active[oracle] = true;
							agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMON);
							num_oracles_active++;
						}
					}
				}
			}
		}
		else
		{
			Units oracles = Units(state_machine->oracles);
			std::sort(oracles.begin(), oracles.end(), [](const Unit* &a, const Unit* &b) -> bool
			{
				return a->energy > b->energy;
			});
			for (const auto &oracle : oracles)
			{
				if (oracle->energy > 10 && (enemy_lings.size() == 0 || Utility::DistanceToClosest(enemy_lings, oracle->pos)))
				{
					if (state_machine->is_beam_active.count(oracle) && state_machine->is_beam_active[oracle] == true)
					{
						state_machine->is_beam_active[oracle] = false;
						agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
					}
				}
			}
		}
		// add oracle to volley or agnore units targetted in volley?
		// add event listeners for oracle
		for (const auto &oracle : state_machine->oracles)
		{
			if (state_machine->casting[oracle])
			{
				if (state_machine->casting_energy[oracle] > oracle->energy || state_machine->casting_energy[oracle] + 5 < oracle->energy)
				{
					state_machine->casting[oracle] = false;
				}
				else
				{
					continue;
				}
			}
			if (state_machine->is_beam_active[oracle] == false)
			{
				agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, center);
				continue;
			}
			float now = agent->Observation()->GetGameLoop() / 22.4;
			bool weapon_ready = now - state_machine->time_last_attacked[oracle] > .8; //.61

			agent->Debug()->DebugTextOut("weapon ready " + std::to_string(weapon_ready), Point2D(.2, .35), Color(0, 255, 0), 20);
			agent->Debug()->DebugTextOut("has attacked " + std::to_string(state_machine->has_attacked[oracle]), Point2D(.2, .37), Color(0, 255, 0), 20);
			agent->Debug()->DebugTextOut("target " + std::to_string(state_machine->target[oracle]), Point2D(.2, .39), Color(0, 255, 0), 20);


			if (weapon_ready)
			{
				const Unit* closest_unit = Utility::ClosestTo(enemy_lings, oracle->pos);
				if (closest_unit == NULL || Distance2D(closest_unit->pos, oracle->pos) > 6)
				{
					agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, center);
					continue;
				}


				agent->Actions()->UnitCommand(oracle, ABILITY_ID::ATTACK_ATTACK, closest_unit);
				agent->Debug()->DebugSphereOut(closest_unit->pos, .75, Color(0, 255, 255));

				state_machine->target[oracle] = closest_unit->tag;
				state_machine->time_last_attacked[oracle] = agent->Observation()->GetGameLoop() / 22.4;
				state_machine->has_attacked[oracle] = false;
				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(255, 0, 0));
			}
			else if (state_machine->has_attacked[oracle])
			{
				agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, center);

				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 0, 255));
			}
			else
			{
				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 255, 0));
			}
		}
		// update beam status for tired oracles
		for (const auto &oracle : state_machine->oracles)
		{
			if (oracle->energy <= 2)
				state_machine->is_beam_active[oracle] = false;
		}

	}

	void OracleDefendArmyGroup::EnterState()
	{
		std::function<void(const Unit*, float, float)> onUnitDamaged = [=](const Unit* unit, float health, float shields) {
			this->OnUnitDamagedListener(unit, health, shields);
		};
		agent->AddListenerToOnUnitDamagedEvent(event_id, onUnitDamaged);

		std::function<void(const Unit*)> onUnitDestroyed = [=](const Unit* unit) {
			this->OnUnitDestroyedListener(unit);
		};
		agent->AddListenerToOnUnitDestroyedEvent(event_id, onUnitDestroyed);
	}

	void OracleDefendArmyGroup::ExitState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
		}
		agent->RemoveListenerToOnUnitDamagedEvent(event_id);
		agent->RemoveListenerToOnUnitDestroyedEvent(event_id);
		return;
	}

	State* OracleDefendArmyGroup::TestTransitions()
	{
		return NULL;
	}

	void OracleDefendArmyGroup::OnUnitDamagedListener(const Unit* unit, float health, float shields)
	{
		std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " took " << std::to_string(health) << " damage\n";
		int i = 0;
		agent->Debug()->DebugTextOut(std::to_string(unit->tag), Point2D(.1, .35), Color(0, 255, 0), 20);

		for (const auto &oracle : state_machine->oracles)
		{
			if (state_machine->target[oracle] == unit->tag)
			{
				agent->Debug()->DebugTextOut(Utility::UnitTypeIdToString(unit->unit_type.ToType()) + " took " + std::to_string(health) + " damage from orale", Point2D(.2, .4 + .02 * i), Color(0, 255, 0), 20);
				std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " took " << std::to_string(health) << " damage from orale\n";
				state_machine->has_attacked[oracle] = true;
			}
			i++;
		}
	}

	void OracleDefendArmyGroup::OnUnitDestroyedListener(const Unit* unit)
	{
		std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed\n";
		int i = 0;
		agent->Debug()->DebugTextOut(std::to_string(unit->tag), Point2D(.1, .39), Color(0, 255, 255), 20);
		for (const auto &oracle : state_machine->oracles)
		{
			if (state_machine->target[oracle] == unit->tag)
			{
				agent->Debug()->DebugTextOut(Utility::UnitTypeIdToString(unit->unit_type.ToType()) + " desroyed by oracle", Point2D(.2, .45 + .02 * i), Color(0, 255, 0), 20);
				std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed by orale\n";
				state_machine->has_attacked[oracle] = true;
			}
			i++;
		}
	}

#pragma endregion


#pragma region OracleCoverArmy

	std::string OracleCoverArmy::toString()
	{
		return "Cover Army";
	}

#pragma endregion

#pragma region OracleHarassGroupUp

	std::string OracleHarassGroupUp::toString()
	{
		return "Oracle Harass Group Up";
	}

	void OracleHarassGroupUp::EnterState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, consolidation_pos);
		}
	}

	State* OracleHarassGroupUp::TestTransitions()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			if (Distance2D(oracle->pos, consolidation_pos) > 1)
			{
				return NULL;
			}
		}
		if (state_machine->harass_direction)
			return new OracleHarassMoveToEntrance(agent, state_machine, agent->locations->oracle_path.entrance_points[state_machine->harass_index]);
		else
			return new OracleHarassMoveToEntrance(agent, state_machine, agent->locations->oracle_path.exit_points[state_machine->harass_index]);

	}

	void OracleHarassGroupUp::TickState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, consolidation_pos);
		}
	}

	void OracleHarassGroupUp::ExitState()
	{
		return;
	}

#pragma endregion

#pragma region OracleHarassMoveToEntrance

	std::string OracleHarassMoveToEntrance::toString()
	{
		return "Oracle Harass Move to Entrance";
	}

	void OracleHarassMoveToEntrance::EnterState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, entrance_pos);
		}
	}

	State* OracleHarassMoveToEntrance::TestTransitions()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			if (Distance2D(oracle->pos, entrance_pos) > 2)
			{
				return NULL;
			}
		}
		if (state_machine->harass_direction)
			return new OracleHarassAttackMineralLine(agent, state_machine, agent->locations->oracle_path.exit_points[state_machine->harass_index]);
		else
			return new OracleHarassAttackMineralLine(agent, state_machine, agent->locations->oracle_path.entrance_points[state_machine->harass_index]);
	}

	void OracleHarassMoveToEntrance::TickState()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			agent->Actions()->UnitCommand(oracle, ABILITY_ID::MOVE_MOVE, entrance_pos);
		}
	}

	void OracleHarassMoveToEntrance::ExitState()
	{
		return;
	}

#pragma endregion

#pragma region OracleHarassAttackMineralLine

	std::string OracleHarassAttackMineralLine::toString()
	{
		return "Oracle Harass Attack Mineral Line";
	}

	void OracleHarassAttackMineralLine::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::BEHAVIOR_PULSARBEAMON);

		std::function<void(const Unit*)> onUnitDestroyed = [=](const Unit* unit) {
			this->OnUnitDestroyedListener(unit);
		};
		agent->AddListenerToOnUnitDestroyedEvent(event_id, onUnitDestroyed);
	}

	State* OracleHarassAttackMineralLine::TestTransitions()
	{
		if (!lost_oracle)
		{
			for (const auto &oracle : state_machine->oracles)
			{
				if (Distance2D(oracle->pos, exit_pos) > 2)
				{
					return NULL;
				}
			}
		}
		if (state_machine->harass_direction)
			return new OracleHarassReturnToBase(agent, state_machine, agent->locations->oracle_path.exfi_paths[state_machine->harass_index]);
		else
			return new OracleHarassReturnToBase(agent, state_machine, agent->locations->oracle_path.exfi_paths[state_machine->harass_index - 1]);
	}
	
	void OracleHarassAttackMineralLine::TickState()
	{
		bool weapons_ready = true;
		for (const auto &oracle : state_machine->oracles)
		{
			float now = agent->Observation()->GetGameLoop() / 22.4;
			bool weapon_ready = now - state_machine->time_last_attacked[oracle] > .8; //.61
			if (!weapon_ready)
			{
				weapons_ready = false;
				break;
			}
		}


		Point2D oracle_center = Utility::MedianCenter(state_machine->oracles);
		float s = (oracle_center.y - exit_pos.y) / (oracle_center.x - exit_pos.x);
		float d = sqrt(pow(oracle_center.x - exit_pos.x, 2) + pow(oracle_center.y - exit_pos.y, 2));
		Point2D l1 = Point2D(oracle_center.x + 2 * (exit_pos.x - oracle_center.x) / d + 3, oracle_center.y + 2 * (exit_pos.y - oracle_center.y) / d - (3 / s));
		Point2D l2 = Point2D(oracle_center.x + 2 * (exit_pos.x - oracle_center.x) / d - 3, oracle_center.y + 2 * (exit_pos.y - oracle_center.y) / d + (3 / s));
		Point3D L1 = agent->ToPoint3D(l1) + Point3D(0, 0, .2);
		Point3D L2 = agent->ToPoint3D(l2) + Point3D(0, 0, .2);

		agent->Debug()->DebugLineOut(L1, L2, Color(255, 0, 255));


		if (weapons_ready)
		{
			Units drones = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::ZERG_DRONE));
			if (drones.size() == 0)
			{
				if (Distance2D(oracle_center, exit_pos) > 4)
					agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(oracle_center, exit_pos, 4));
				else
					agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exit_pos);
				//agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exit_pos);
				agent->Debug()->DebugSphereOut(state_machine->oracles[0]->pos, 1, Color(0, 255, 255));
				return;
			}

			Point2D oracle_center = Utility::MedianCenter(state_machine->oracles);
			float perp_direction_to_exit = -1 / sqrt(pow(oracle_center.x + exit_pos.x, 2) + pow(oracle_center.y + exit_pos.y, 2));

			float best_angle = 145;
			Point2D ideal_pos = Utility::PointBetween(oracle_center, exit_pos, 2);
			Point2D exit_vector = Point2D(exit_pos.x - ideal_pos.x, exit_pos.y - ideal_pos.y);
			agent->Debug()->DebugSphereOut(agent->ToPoint3D(ideal_pos), 1, Color(255, 255, 0));

			for (const auto &extractor : agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::ZERG_EXTRACTOR)))
			{
				agent->Debug()->DebugSphereOut(extractor->pos, 3, Color(255, 0, 255));
			}

			for (const auto &drone : drones)
			{
				if (Utility::DistanceToClosest(agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::ZERG_EXTRACTOR)), drone->pos) < 3)
					continue;
				Point2D drone_vector = Point2D(drone->pos.x - ideal_pos.x, drone->pos.y - ideal_pos.y);
				float dot_product = exit_vector.x * drone_vector.x + exit_vector.y * drone_vector.y;
				float exit_mag = sqrt(pow(exit_vector.x, 2) + pow(exit_vector.y, 2));
				float drone_mag = sqrt(pow(drone_vector.x, 2) + pow(drone_vector.y, 2));

				float angle = acos(dot_product / (exit_mag * drone_mag));

				agent->Debug()->DebugTextOut(std::to_string(round(angle * 57)), drone->pos, Color(255, 255, 255), 20);

				if (angle < best_angle)
				{
					bool out_of_range = false;
					for (const auto &oracle : state_machine->oracles)
					{
						if (Distance2D(oracle->pos, drone->pos) > 4)
						{
							out_of_range = true;
							break;
						}
					}
					if (out_of_range)
						continue;
					target_drone = drone;
					best_angle = angle;
				}
			}
			if (target_drone == NULL)
			{
				if (Distance2D(oracle_center, exit_pos) > 4)
					agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(oracle_center, exit_pos, 4));
				else
					agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exit_pos);
				//agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exit_pos);
				agent->Debug()->DebugSphereOut(state_machine->oracles[0]->pos, 1, Color(0, 255, 255));
				return;
			}


			agent->Debug()->DebugTextOut(std::to_string(target_drone->tag), Point2D(.5, .5), Color(255, 0, 255), 20);
			agent->Debug()->DebugSphereOut(target_drone->pos, 1, Color(0, 255, 255));


			agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::ATTACK_ATTACK, target_drone);


			for (const auto &oracle : state_machine->oracles)
			{
				state_machine->time_last_attacked[oracle] = agent->Observation()->GetGameLoop() / 22.4;
				state_machine->has_attacked[oracle] = false;
				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(255, 0, 0));
			}
		}
		else if (state_machine->has_attacked[state_machine->oracles[0]])
		{
			if (Distance2D(oracle_center, exit_pos) > 4)
				agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(oracle_center, exit_pos, 4));
			else
				agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exit_pos);
			//agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::GENERAL_MOVE, exit_pos);

			for (const auto &oracle : state_machine->oracles)
			{
				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 0, 255));
			}
		}
		else
		{
			for (const auto &oracle : state_machine->oracles)
			{
				agent->Debug()->DebugSphereOut(oracle->pos, 2, Color(0, 255, 0));
			}
		}
	}

	void OracleHarassAttackMineralLine::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
		agent->RemoveListenerToOnUnitDestroyedEvent(event_id);
	}

	void OracleHarassAttackMineralLine::OnUnitDestroyedListener(const Unit* unit)
	{
		std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed\n";
		if (target_drone != NULL && unit->tag == target_drone->tag)
		{
			target_drone = NULL;
			for (const auto &oracle : state_machine->oracles)
			{
				state_machine->has_attacked[oracle] = true;
				/*
				if (state_machine->oracles[i]->engaged_target_tag == unit->tag || state_machine->oracles[i]->engaged_target_tag == 0)
				{
					std::cout << Utility::UnitTypeIdToString(unit->unit_type.ToType()) << " destroyed by orale\n";
					state_machine->has_attacked[i] = true;
				}*/
			}
			agent->Debug()->DebugTextOut(std::to_string(unit->tag), Point2D(.5, .45), Color(0, 255, 255), 20);

		}
	}

#pragma endregion

#pragma region OracleHarassReturnToBase

	std::string OracleHarassReturnToBase::toString()
	{
		return "Oracle Harass Return to Base";
	}

	void OracleHarassReturnToBase::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::BEHAVIOR_PULSARBEAMOFF);
		for (int i = 0; i < exfil_path.size(); i++)
		{
			agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, exfil_path[i], i > 0);
		}
		agent->Actions()->UnitCommand(state_machine->oracles, ABILITY_ID::MOVE_MOVE, agent->locations->start_location, true);
	}

	State* OracleHarassReturnToBase::TestTransitions()
	{
		for (const auto &oracle : state_machine->oracles)
		{
			if (Distance2D(oracle->pos, agent->locations->start_location) > 20)
				return NULL;
		}
		if (state_machine->attached_army_group == NULL)
			return NULL;
		return new OracleDefendArmyGroup(agent, state_machine);
	}

	void OracleHarassReturnToBase::TickState()
	{
		return;
	}

	void OracleHarassReturnToBase::ExitState()
	{
		return;
	}

#pragma endregion

#pragma region ChargeAllInMovingToWarpinSpot

	std::string ChargeAllInMovingToWarpinSpot::toString()
	{
		float time_left = state_machine->last_warp_in_time + 20 - agent->Observation()->GetGameLoop() / 22.4;
		return "ChargeAllIn looking for warp in spot " + std::to_string((int)time_left);
	}

	void ChargeAllInMovingToWarpinSpot::TickState()
	{
		// move prism to spot
		if (Distance2D(state_machine->prism->pos, state_machine->next_warp_in_location) > 1)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->next_warp_in_location);
		for (const auto &zealot : state_machine->zealots)
		{
			if (zealot->orders.size() == 0)
			{
				Point2D pos = agent->Observation()->GetGameInfo().enemy_start_locations[0];
				agent->Actions()->UnitCommand(zealot, ABILITY_ID::ATTACK_ATTACK, pos);
			}
		}
		return;
	}

	void ChargeAllInMovingToWarpinSpot::EnterState()
	{
		// find warp in spot
		state_machine->prism_spots_index++;
		if (state_machine->prism_spots_index >= state_machine->prism_spots.size())
			state_machine->prism_spots_index = 0;
		state_machine->next_warp_in_location = state_machine->prism_spots[state_machine->prism_spots_index];
	}

	void ChargeAllInMovingToWarpinSpot::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MORPH_WARPPRISMPHASINGMODE);
	}

	State* ChargeAllInMovingToWarpinSpot::TestTransitions()
	{
		float time_left = state_machine->last_warp_in_time + 20 - agent->Observation()->GetGameLoop() / 22.4;
		if (time_left < 2)
			return new ChargeAllInWarpingIn(agent, state_machine);
		return NULL;
	}

#pragma endregion

#pragma region ChargeAllInWarpingIn

	std::string ChargeAllInWarpingIn::toString()
	{
		return "ChargeAllIn warping in";
	}

	void ChargeAllInWarpingIn::TickState()
	{
		for (const auto &zealot : state_machine->zealots)
		{
			if (zealot->orders.size() == 0)
			{
				Point2D pos = agent->Observation()->GetGameInfo().enemy_start_locations[0];
				agent->Actions()->UnitCommand(zealot, ABILITY_ID::ATTACK_ATTACK, pos);
			}
		}

		bool all_gates_ready = true;
		Units gates = agent->Observation()->GetUnits(IsFinishedUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
		for (const auto &warpgate : gates)
		{
			if (agent->warpgate_status[warpgate].frame_ready > 0)
			{
				all_gates_ready = false;
				break;
			}
		}
		if (gates.size() > 0 && all_gates_ready && Utility::CanAfford(UNIT_TYPEID::PROTOSS_ZEALOT, gates.size(), agent->Observation()))
		{
			std::vector<Point2D> spots = agent->FindWarpInSpots(agent->Observation()->GetGameInfo().enemy_start_locations[0], gates.size());
			if (spots.size() >= gates.size())
			{
				for (int i = 0; i < gates.size(); i++)
				{
					Point3D pos = Point3D(gates[i]->pos.x, gates[i]->pos.y, agent->Observation()->TerrainHeight(gates[i]->pos));
					agent->Debug()->DebugSphereOut(pos, 1, Color(255, 0, 255));
					agent->Actions()->UnitCommand(gates[i], ABILITY_ID::TRAINWARP_ZEALOT, spots[i]);
					agent->warpgate_status[gates[i]].used = true;
					agent->warpgate_status[gates[i]].frame_ready = agent->Observation()->GetGameLoop() + round(20 * 22.4);
					state_machine->last_warp_in_time = agent->Observation()->GetGameLoop() / 22.4;
				}
			}
		}
	}

	void ChargeAllInWarpingIn::EnterState()
	{
		if (state_machine->prism->unit_type != UNIT_TYPEID::PROTOSS_WARPPRISMPHASING)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MORPH_WARPPRISMPHASINGMODE);
	}

	void ChargeAllInWarpingIn::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MORPH_WARPPRISMTRANSPORTMODE);
	}

	State* ChargeAllInWarpingIn::TestTransitions()
	{
		float time_left = state_machine->last_warp_in_time + 20 - agent->Observation()->GetGameLoop() / 22.4;
		if (time_left < 16 && time_left > 10)
			return new ChargeAllInMovingToWarpinSpot(agent, state_machine);
		// if last warp in time >3 and < 8
		// return new ChargeAllInLookingForWarpinSpot)(
		return NULL;
	}

#pragma endregion


#pragma region ScoutZInitialMove

	void ScoutZInitialMove::TickState()
	{
		if (state_machine->scout->orders.size() == 0 || state_machine->scout->orders[0].target_pos != state_machine->current_target)
			agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutZInitialMove::EnterState()
	{
		state_machine->current_target = state_machine->enemy_main;
	}

	void ScoutZInitialMove::ExitState()
	{
		return;
	}

	State* ScoutZInitialMove::TestTransitions()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 1)
			return new ScoutZScoutMain(agent, state_machine);
		return NULL;
	}

	std::string ScoutZInitialMove::toString()
	{
		return "initial move";
	}

#pragma endregion

#pragma region ScoutZScoutMain

	void ScoutZScoutMain::TickState()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 3)
		{
			state_machine->index++;
			if (state_machine->index < state_machine->main_scout_path.size())
				state_machine->current_target = state_machine->main_scout_path[state_machine->index];
		}
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutZScoutMain::EnterState()
	{
		state_machine->index = 0;
		state_machine->current_target = state_machine->main_scout_path[0];
	}

	void ScoutZScoutMain::ExitState()
	{
		return;
	}

	State* ScoutZScoutMain::TestTransitions()
	{
		if (state_machine->index >= state_machine->main_scout_path.size())
			return new ScoutZScoutNatural(agent, state_machine);
		return NULL;
	}

	std::string ScoutZScoutMain::toString()
	{
		return "scout main";
	}

#pragma endregion

#pragma region ScoutZScoutNatural

	void ScoutZScoutNatural::TickState()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 3)
		{
			state_machine->index++;
			if (state_machine->index < state_machine->natural_scout_path.size())
				state_machine->current_target = state_machine->natural_scout_path[state_machine->index];
		}
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutZScoutNatural::EnterState()
	{
		state_machine->index = 0;
		state_machine->current_target = state_machine->natural_scout_path[0];
	}

	void ScoutZScoutNatural::ExitState()
	{
		return;
	}

	State* ScoutZScoutNatural::TestTransitions()
	{
		if (state_machine->index >= state_machine->natural_scout_path.size())
			return new ScoutZLookFor3rd(agent, state_machine);
		return NULL;
	}

	std::string ScoutZScoutNatural::toString()
	{
		return "scout natural";
	}

#pragma endregion

#pragma region ScoutZLookFor3rd

	void ScoutZLookFor3rd::TickState()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 8)
		{
			state_machine->index++;
			if (state_machine->index < state_machine->possible_3rds.size())
				state_machine->current_target = state_machine->possible_3rds[state_machine->index];
		}
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutZLookFor3rd::EnterState()
	{
		state_machine->index = 0;
		state_machine->current_target = state_machine->possible_3rds[0];
	}

	void ScoutZLookFor3rd::ExitState()
	{
		return;
	}
	
	State* ScoutZLookFor3rd::TestTransitions()
	{
		if (state_machine->index >= state_machine->possible_3rds.size())
			return new ScoutZScoutMain(agent, state_machine);
		return NULL;
	}

	std::string ScoutZLookFor3rd::toString()
	{
		return "look for 3rd";
	}

#pragma endregion


#pragma region ScoutTInitialMove

	void ScoutTInitialMove::TickState()
	{
		if (state_machine->scout->orders.size() == 0 || state_machine->scout->orders[0].target_pos != state_machine->current_target)
			agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutTInitialMove::EnterState()
	{
		state_machine->current_target = state_machine->enemy_main;
	}

	void ScoutTInitialMove::ExitState()
	{
		return;
	}

	State* ScoutTInitialMove::TestTransitions()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 1)
			return new ScoutTScoutMain(agent, state_machine);
		return NULL;
	}

	std::string ScoutTInitialMove::toString()
	{
		return "initial move";
	}

#pragma endregion

#pragma region ScoutTScoutMain

	void ScoutTScoutMain::TickState()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 3)
		{
			state_machine->index++;
			if (state_machine->index < state_machine->main_scout_path.size())
				state_machine->current_target = state_machine->main_scout_path[state_machine->index];
		}
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutTScoutMain::EnterState()
	{
		state_machine->index = 0;
		state_machine->current_target = state_machine->main_scout_path[0];
	}

	void ScoutTScoutMain::ExitState()
	{
		return;
	}

	State* ScoutTScoutMain::TestTransitions()
	{
		if (agent->scout_info_terran.barrackes_timing > 0 && agent->Observation()->GetGameLoop() / 22.4 >= agent->scout_info_terran.barrackes_timing + 46 + 12)
		{
			return new ScoutTScoutRax(agent, state_machine);
		}
		if (state_machine->index >= state_machine->main_scout_path.size())
		{
			if (agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_BARRACKS)).size() > 1)
			{
				state_machine->index = 0;
				state_machine->current_target = state_machine->main_scout_path[0];
			}
			else if (agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_REFINERY)).size() <= 1)
			{
				return new ScoutTScoutNatural(agent, state_machine);
			}
			else if (agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_REFINERY)).size() > 1)
			{
				state_machine->index = 0;
				state_machine->current_target = state_machine->main_scout_path[0];
			}
		}
		return NULL;
	}

	std::string ScoutTScoutMain::toString()
	{
		return "scout main";
	}

#pragma endregion

#pragma region ScoutTScoutNatural

	void ScoutTScoutNatural::TickState()
	{
		if (Distance2D(state_machine->scout->pos, state_machine->current_target) < 3)
		{
			state_machine->index++;
			if (state_machine->index < state_machine->natural_scout_path.size())
				state_machine->current_target = state_machine->natural_scout_path[state_machine->index];
		}
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, state_machine->current_target);
	}

	void ScoutTScoutNatural::EnterState()
	{
		state_machine->index = 0;
		state_machine->current_target = state_machine->natural_scout_path[0];
	}

	void ScoutTScoutNatural::ExitState()
	{
		return;
	}

	State* ScoutTScoutNatural::TestTransitions()
	{
		if (agent->scout_info_terran.natural_timing > 0 || state_machine->index >= state_machine->natural_scout_path.size())
		{
			return new ScoutTScoutMain(agent, state_machine);
		}
		return NULL;
	}

	std::string ScoutTScoutNatural::toString()
	{
		return "scout natural";
	}

#pragma endregion

#pragma region ScoutTScoutRax

	void ScoutTScoutRax::TickState()
	{

	}

	void ScoutTScoutRax::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_BARRACKS))[0]->pos);
		rax = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_BARRACKS))[0];
	}

	void ScoutTScoutRax::ExitState()
	{
		return;
	}

	State* ScoutTScoutRax::TestTransitions()
	{
		if (agent->Observation()->GetGameLoop() / 22.4 >= agent->scout_info_terran.barrackes_timing + 46 + 20 || agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_MARINE)).size() > 0)
		{
			agent->scout_info_terran.first_rax_production = FirstRaxProduction::reaper;

			for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
			{
				if (unit->unit_type == UNIT_TYPEID::TERRAN_BARRACKSTECHLAB)
				{
					if (Distance2D(unit->pos, rax->pos) < 3)
					{
						agent->scout_info_terran.first_rax_production = FirstRaxProduction::techlab;
						break;
					}
				}
				else if (unit->unit_type == UNIT_TYPEID::TERRAN_BARRACKSREACTOR)
				{
					if (Distance2D(unit->pos, rax->pos) < 3)
					{
						agent->scout_info_terran.first_rax_production = FirstRaxProduction::reactor;
						break;
					}
				}
				else if (unit->unit_type == UNIT_TYPEID::TERRAN_MARINE)
				{
					agent->scout_info_terran.first_rax_production = FirstRaxProduction::marine;
					break;
				}
			}
			return new ScoutTReturnToBase(agent, state_machine);
		}
		return NULL;
	}

	std::string ScoutTScoutRax::toString()
	{
		return "scout rax";
	}

#pragma endregion

#pragma region ScoutTReturnToBase

	void ScoutTReturnToBase::TickState()
	{
		for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
		{
			if (unit->unit_type == UNIT_TYPEID::TERRAN_MARINE)
			{
				agent->scout_info_terran.first_rax_production = FirstRaxProduction::marine;
				break;
			}
		}
		return;
	}

	void ScoutTReturnToBase::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->scout, ABILITY_ID::MOVE_MOVE, agent->locations->start_location);
	}

	void ScoutTReturnToBase::ExitState()
	{
		return;
	}

	State* ScoutTReturnToBase::TestTransitions()
	{
		if (Distance2D(state_machine->scout->pos, agent->locations->start_location) <= 20)
			state_machine->CloseStateMachine();
		return NULL;
	}

	std::string ScoutTReturnToBase::toString()
	{
		return "return to base";
	}

#pragma endregion


#pragma region ImmortalDropWaitForImmortals

	void ImmortalDropWaitForImmortals::TickState()
	{
		if (state_machine->prism == NULL)
		{
			for (const auto &prism : agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::PROTOSS_WARPPRISM)))
			{
				state_machine->prism = prism;
				break;
			}
		}
		if (state_machine->immortal1 == NULL || state_machine->immortal2 == NULL)
		{
			for (const auto &immortal : agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::PROTOSS_IMMORTAL)))
			{
				if (state_machine->immortal1 == NULL && immortal != state_machine->immortal2)
					state_machine->immortal1 = immortal;
				else if (state_machine->immortal2 == NULL && immortal != state_machine->immortal1)
					state_machine->immortal2 = immortal;
			}
		}
	}

	void ImmortalDropWaitForImmortals::EnterState()
	{
		return;
	}

	void ImmortalDropWaitForImmortals::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::SMART, state_machine->prism);
		agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::SMART, state_machine->prism);
		return;
	}

	State* ImmortalDropWaitForImmortals::TestTransitions()
	{
		if (state_machine->prism != NULL && state_machine->immortal1 != NULL  && state_machine->immortal2 != NULL)
			return new ImmortalDropInitialMove(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropWaitForImmortals::toString()
	{
		return "wait for immortals";
	}

#pragma endregion

#pragma region ImmortalDropInitialMove

	void ImmortalDropInitialMove::TickState()
	{
		agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->entry_pos);
	}

	void ImmortalDropInitialMove::EnterState()
	{
		return;
	}

	void ImmortalDropInitialMove::ExitState()
	{
		return;
	}

	State* ImmortalDropInitialMove::TestTransitions()
	{
		if (Distance2D(state_machine->prism->pos, state_machine->entry_pos) < 15)
			return new ImmortalDropMicroDrop(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropInitialMove::toString()
	{
		return "initial move";
	}

#pragma endregion

#pragma region ImmortalDropMicroDrop

	void ImmortalDropMicroDrop::TickState()
	{
		agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::UNLOADALLAT, state_machine->prism);
		agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal1->pos, state_machine->prism->pos, 1));
		agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal2->pos, state_machine->prism->pos, 1));
	}


	void ImmortalDropMicroDrop::EnterState()
	{
		return;
	}

	void ImmortalDropMicroDrop::ExitState()
	{
		return;
	}

	State* ImmortalDropMicroDrop::TestTransitions()
	{
		if (state_machine->prism->cargo_space_taken == 0)
			return new ImmortalDropMicroDropDropped2(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropMicroDrop::toString()
	{
		return "micro immortal drop";
	}

#pragma endregion

#pragma region ImmortalDropMicroDropCarrying1

	void ImmortalDropMicroDropCarrying1::TickState()
	{
		// don't leave immortals behind
		if (Distance2D(state_machine->prism->pos, state_machine->immortal2->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal2->pos);
		else
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->UpdatePrismPathing());

		if (agent->Observation()->GetGameLoop() >= entry_frame + 15)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::UNLOADALLAT, state_machine->prism);
		if (state_machine->immortal1->orders.size() == 0 || state_machine->immortal1->orders[0].ability_id != ABILITY_ID::SMART)
			agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal1->pos, state_machine->prism->pos, 1));
		if (state_machine->immortal2->orders.size() == 0 || state_machine->immortal2->orders[0].ability_id != ABILITY_ID::SMART)
			agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal2->pos, state_machine->prism->pos, 1));
	}

	void ImmortalDropMicroDropCarrying1::EnterState()
	{
		return;
	}

	void ImmortalDropMicroDropCarrying1::ExitState()
	{
		return;
	}

	State* ImmortalDropMicroDropCarrying1::TestTransitions()
	{
		if (state_machine->immortal1->weapon_cooldown == 0 && state_machine->immortal2->weapon_cooldown == 0 && state_machine->prism->cargo_space_taken == 0)
			return new ImmortalDropMicroDropDropped1(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropMicroDropCarrying1::toString()
	{
		return "micro immortal drop carrying 1";
	}

#pragma endregion

#pragma region ImmortalDropMicroDropCarrying2

	void ImmortalDropMicroDropCarrying2::TickState()
	{
		// don't leave immortals behind
		if (Distance2D(state_machine->prism->pos, state_machine->immortal1->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal1->pos);
		else
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->UpdatePrismPathing());

		if (agent->Observation()->GetGameLoop() >= entry_frame + 15)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::UNLOADALLAT, state_machine->prism);
		if (state_machine->immortal1->orders.size() == 0 || state_machine->immortal1->orders[0].ability_id != ABILITY_ID::SMART)
			agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal1->pos, state_machine->prism->pos, 1));
		if (state_machine->immortal2->orders.size() == 0 || state_machine->immortal2->orders[0].ability_id != ABILITY_ID::SMART)
			agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::MOVE_MOVE, Utility::PointBetween(state_machine->immortal2->pos, state_machine->prism->pos, 1));
	}

	void ImmortalDropMicroDropCarrying2::EnterState()
	{
		return;
	}

	void ImmortalDropMicroDropCarrying2::ExitState()
	{
		return;
	}

	State* ImmortalDropMicroDropCarrying2::TestTransitions()
	{
		if (state_machine->immortal1->weapon_cooldown == 0 && state_machine->immortal2->weapon_cooldown == 0 && state_machine->prism->cargo_space_taken == 0)
			return new ImmortalDropMicroDropDropped2(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropMicroDropCarrying2::toString()
	{
		return "micro immortal drop carrying 2";
	}

#pragma endregion

#pragma region ImmortalDropMicroDropDropped1

	void ImmortalDropMicroDropDropped1::TickState()
	{
		// don't leave immortals behind
		if (Distance2D(state_machine->prism->pos, state_machine->immortal1->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal1->pos);
		else if (Distance2D(state_machine->prism->pos, state_machine->immortal2->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal2->pos);
		else
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->UpdatePrismPathing());

		if ((!immortal1_has_attack_order || state_machine->immortal1->weapon_cooldown > 0) && (!immortal2_has_attack_order || state_machine->immortal2->weapon_cooldown > 0))
			agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::SMART, state_machine->prism);
	}

	void ImmortalDropMicroDropDropped1::EnterState()
	{
		std::map<const Unit*, const Unit*> attacks = agent->FindTargets({ state_machine->immortal1, state_machine->immortal2 }, state_machine->target_priority, 0);
		if (attacks.count(state_machine->immortal1))
		{
			agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::ATTACK, attacks[state_machine->immortal1]);
			immortal1_has_attack_order = true;
		}

		if (attacks.count(state_machine->immortal2))
		{
			agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::ATTACK, attacks[state_machine->immortal2]);
			immortal2_has_attack_order = true;
		}
	}

	void ImmortalDropMicroDropDropped1::ExitState()
	{
		
	}

	State* ImmortalDropMicroDropDropped1::TestTransitions()
	{
		if (state_machine->prism->cargo_space_taken == 4)
			return new ImmortalDropMicroDropCarrying2(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropMicroDropDropped1::toString()
	{
		return "micro immortal drop dropped 1";
	}

#pragma endregion

#pragma region ImmortalDropMicroDropDropped2

	void ImmortalDropMicroDropDropped2::TickState()
	{
		// don't leave immortals behind
		if (Distance2D(state_machine->prism->pos, state_machine->immortal1->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal1->pos);
		else if (Distance2D(state_machine->prism->pos, state_machine->immortal2->pos) > 4.5)
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->immortal2->pos);
		else
			agent->Actions()->UnitCommand(state_machine->prism, ABILITY_ID::MOVE_MOVE, state_machine->UpdatePrismPathing());

		if ((!immortal1_has_attack_order || state_machine->immortal1->weapon_cooldown > 0) && (!immortal2_has_attack_order || state_machine->immortal2->weapon_cooldown > 0))
			agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::SMART, state_machine->prism);
	}

	void ImmortalDropMicroDropDropped2::EnterState()
	{
		std::map<const Unit*, const Unit*> attacks = agent->FindTargets({ state_machine->immortal1, state_machine->immortal2 }, state_machine->target_priority, 0);
		if (attacks.count(state_machine->immortal1))
		{
			agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::ATTACK, attacks[state_machine->immortal1]);
			immortal1_has_attack_order = true;
		}

		if (attacks.count(state_machine->immortal2))
		{
			agent->Actions()->UnitCommand(state_machine->immortal2, ABILITY_ID::ATTACK, attacks[state_machine->immortal2]);
			immortal2_has_attack_order = true;
		}
	}

	void ImmortalDropMicroDropDropped2::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->immortal1, ABILITY_ID::SMART, state_machine->prism);
	}

	State* ImmortalDropMicroDropDropped2::TestTransitions()
	{
		if (state_machine->prism->cargo_space_taken == 4)
			return new ImmortalDropMicroDropCarrying1(agent, state_machine);
		return NULL;
	}

	std::string ImmortalDropMicroDropDropped2::toString()
	{
		return "micro immortal drop dropped 2";
	}

#pragma endregion


#pragma region DoorOpen

	void DoorOpen::TickState()
	{
		return;
	}

	void DoorOpen::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->guard, ABILITY_ID::MOVE_MOVE, state_machine->door_open_pos);
		agent->Actions()->UnitCommand(state_machine->guard, ABILITY_ID::GENERAL_HOLDPOSITION, true);
	}

	void DoorOpen::ExitState()
	{
		return;
	}

	State* DoorOpen::TestTransitions()
	{
		if (agent->Observation()->GetUnits(Unit::Alliance::Enemy).size() > 0 && Utility::DistanceToClosest(agent->Observation()->GetUnits(Unit::Alliance::Enemy), state_machine->guard->pos) < 8)
			return new DoorClosed(agent, state_machine);
		return NULL;
	}

	std::string DoorOpen::toString()
	{
		return "door open";
	}

#pragma endregion

#pragma region DoorClosed

	void DoorClosed::TickState()
	{
		return;
	}

	void DoorClosed::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->guard, ABILITY_ID::MOVE_MOVE, state_machine->door_closed_pos);
		agent->Actions()->UnitCommand(state_machine->guard, ABILITY_ID::GENERAL_HOLDPOSITION, true);
	}

	void DoorClosed::ExitState()
	{
		return;
	}

	State* DoorClosed::TestTransitions()
	{
		if (agent->Observation()->GetUnits(Unit::Alliance::Enemy).size() > 0 && Utility::DistanceToClosest(agent->Observation()->GetUnits(Unit::Alliance::Enemy), state_machine->guard->pos) > 8)
			return new DoorOpen(agent, state_machine);
		return NULL;
	}

	std::string DoorClosed::toString()
	{
		return "door closed";
	}

#pragma endregion


#pragma region AdeptBaseDefenseTerranClearBase

	// TODO adjust distances 4, 20, 15
	void AdeptBaseDefenseTerranClearBase::TickState()
	{
		if (checked_dead_space == false)
		{
			if (Distance2D(state_machine->adept->pos, state_machine->dead_space_spot) < 4)
				checked_dead_space = true;
			else
				return;
		}
		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
			{
				if (Distance2D(unit->pos, agent->locations->start_location) < 20 || Distance2D(unit->pos, agent->locations->nexi_locations[1]) < 15)
				{
					state_machine->target = unit;
					break;
				}
			}
			if (state_machine->target == NULL)
			{
				agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->front_of_base[0]);
			}
		}
		else
		{
			if (state_machine->attack_status == false)
			{
				// TODO move infront of units based on distance away
				if (Distance2D(state_machine->target->pos, state_machine->adept->pos) <= 4 && state_machine->adept->weapon_cooldown == 0)
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::ATTACK_ATTACK, state_machine->target);
					state_machine->attack_status = true;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);
					if (state_machine->frame_shade_used + 225 < agent->Observation()->GetGameLoop())
						agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::EFFECT_ADEPTPHASESHIFT, state_machine->target->pos);

				}
			}
			else if (state_machine->adept->weapon_cooldown > 0)
			{
				state_machine->attack_status = false;
			}
			if (state_machine->shade != NULL)
			{
				agent->Actions()->UnitCommand(state_machine->shade, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);
			}
		}
	}

	void AdeptBaseDefenseTerranClearBase::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->dead_space_spot);
	}

	void AdeptBaseDefenseTerranClearBase::ExitState()
	{
		return;
	}

	State* AdeptBaseDefenseTerranClearBase::TestTransitions()
	{
		if (Distance2D(state_machine->adept->pos, state_machine->front_of_base[0]) < 5 || Distance2D(state_machine->adept->pos, state_machine->front_of_base[1]) < 5)
			return new AdeptBaseDefenseTerranDefendFront(agent, state_machine);

		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
			{
				if (Distance2D(unit->pos, agent->locations->start_location) < 20 || Distance2D(unit->pos, agent->locations->nexi_locations[0]) < 15)
				{
					state_machine->target = unit;
					break;
				}
			}
		}
		return NULL;
	}

	std::string AdeptBaseDefenseTerranClearBase::toString()
	{
		return "clear base";
	}

#pragma endregion

#pragma region AdeptBaseDefenseTerranDefendFront

	// TODO change for maps with natural ramp
	void AdeptBaseDefenseTerranDefendFront::TickState()
	{
		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_REAPER)))
			{
				state_machine->target = unit;
				break;
			}
			if (state_machine->target == NULL)
			{
				if (forward)
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->front_of_base[1]);
					if (Distance2D(state_machine->adept->pos, state_machine->front_of_base[1]) < 1)
						forward = false;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->front_of_base[0]);
					if (Distance2D(state_machine->adept->pos, state_machine->front_of_base[0]) < 1)
						forward = true;
				}
			}
		}
		else
		{
			if (state_machine->attack_status == false)
			{
				// TODO move infront of units based on distance away
				if (Distance2D(state_machine->target->pos, state_machine->adept->pos) <= 4 && state_machine->adept->weapon_cooldown == 0)
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::ATTACK_ATTACK, state_machine->target);
					state_machine->attack_status = true;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);
					if (state_machine->frame_shade_used + 225 < agent->Observation()->GetGameLoop())
						agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::EFFECT_ADEPTPHASESHIFT, state_machine->target->pos);

				}
			}
			else if (state_machine->adept->weapon_cooldown > 0)
			{
				state_machine->attack_status = false;
			}
			if (state_machine->shade != NULL)
			{
				agent->Actions()->UnitCommand(state_machine->shade, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);
			}
		}

	}

	void AdeptBaseDefenseTerranDefendFront::EnterState()
	{
		return;
	}

	void AdeptBaseDefenseTerranDefendFront::ExitState()
	{
		return;
	}

	State* AdeptBaseDefenseTerranDefendFront::TestTransitions()
	{
		Units gates = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
		Units other_units = agent->Observation()->GetUnits(IsUnits({ UNIT_TYPEID::PROTOSS_ADEPT, UNIT_TYPEID::PROTOSS_STALKER }));
		if (other_units.size() > 1 || agent->scout_info_terran.first_rax_production != reaper || (gates.size() > 0 && gates[0]->orders.size() > 0 && gates[0]->orders[0].progress > .9))
			return new AdeptBaseDefenseTerranMoveAcross(agent, state_machine);
		return NULL;
	}

	std::string AdeptBaseDefenseTerranDefendFront::toString()
	{
		return "defend front";
	}

#pragma endregion

#pragma region AdeptBaseDefenseTerranMoveAcross

	void AdeptBaseDefenseTerranMoveAcross::TickState()
	{
		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
			{
				if (Distance2D(state_machine->adept->pos, unit->pos) < 8)
				{
					state_machine->target = unit;
					break;
				}
			}
			if (state_machine->target == NULL)
			{
				agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, agent->locations->adept_scout_runaway);
				if (state_machine->frame_shade_used + 225 < agent->Observation()->GetGameLoop())
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::EFFECT_ADEPTPHASESHIFT, agent->locations->adept_scout_runaway);

				if (state_machine->shade != NULL)
					agent->Actions()->UnitCommand(state_machine->shade, ABILITY_ID::MOVE_MOVE, agent->locations->adept_scout_runaway);
			}
		}
		else
		{
			if (Distance2D(state_machine->adept->pos, state_machine->target->pos) > 8)
			{
				state_machine->target = NULL;
				return;
			}

			if (state_machine->attack_status == false)
			{
				// TODO move infront of units based on distance away
				if (Distance2D(state_machine->target->pos, state_machine->adept->pos) <= 4 && state_machine->adept->weapon_cooldown == 0)
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::ATTACK_ATTACK, state_machine->target);
					state_machine->attack_status = true;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);
					if (state_machine->frame_shade_used + 225 < agent->Observation()->GetGameLoop())
						agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::EFFECT_ADEPTPHASESHIFT, agent->locations->adept_scout_runaway);

				}
			}
			else if (state_machine->adept->weapon_cooldown > 0)
			{
				state_machine->attack_status = false;
			}
			if (state_machine->shade != NULL)
			{
				agent->Actions()->UnitCommand(state_machine->shade, ABILITY_ID::MOVE_MOVE, agent->locations->adept_scout_runaway);
			}
		}
	}

	void AdeptBaseDefenseTerranMoveAcross::EnterState()
	{
		return;
	}

	void AdeptBaseDefenseTerranMoveAcross::ExitState()
	{
		return;
	}

	State* AdeptBaseDefenseTerranMoveAcross::TestTransitions()
	{
		if (Distance2D(state_machine->adept->pos, agent->locations->adept_scout_runaway) < 3)
			return new AdeptBaseDefenseTerranScoutBase(agent, state_machine, agent->locations->adept_scout_shade, agent->locations->adept_scout_runaway,
				agent->locations->adept_scout_ramptop, agent->locations->adept_scout_nat_path, agent->locations->adept_scout_base_spots);
		return NULL;
	}

	std::string AdeptBaseDefenseTerranMoveAcross::toString()
	{
		return "move across map";
	}

#pragma endregion

#pragma region AdeptBaseDefenseTerranScoutBase

	void AdeptBaseDefenseTerranScoutBase::TickState()
	{
		Point2D furthest_point = adept_scout_shade;
		Units enemy_units = agent->Observation()->GetUnits(IsFightingUnit(Unit::Alliance::Enemy));
		for (const auto &unit : enemy_units)
		{
			double range = Utility::RealGroundRange(unit, state_machine->adept) + 2;
			Point2D intersection = agent->locations->attack_path_line.GetPointFrom(unit->pos, range, false);
			if (intersection != Point2D(0, 0))
			{
				float dist = Distance2D(intersection, adept_scout_shade);
				if (dist > Distance2D(furthest_point, adept_scout_shade))
				{
					furthest_point = intersection;
				}
			}
		}

		agent->Debug()->DebugSphereOut(agent->ToPoint3D(furthest_point), .5, Color(255, 255, 0));

		const Unit* closest_unit = Utility::ClosestTo(agent->Observation()->GetUnits(Unit::Alliance::Enemy), state_machine->adept->pos);

		if (state_machine->attack_status == true)
		{
			if (state_machine->adept->weapon_cooldown > 0)
				state_machine->attack_status = false;
		}
		else if (state_machine->adept->shield < 5 || state_machine->adept->weapon_cooldown > 0)
		{
			if (state_machine->adept->shield < 5)
				shields_regening = true;
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, furthest_point);
		}
		else if (shields_regening)
		{
			if (state_machine->adept->shield >= 65)
				shields_regening = false;
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, furthest_point);
		}
		else if (Distance2D(closest_unit->pos, state_machine->adept->pos) <= 4)
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::ATTACK_ATTACK, closest_unit);
			state_machine->attack_status = true;
		}
		else
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, closest_unit);
		}
		/*else if (Distance2D(closest_unit->pos, state_machine->adept->pos) <= 4)
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::ATTACK_ATTACK, closest_unit);
			state_machine->attack_status = true;
		}
		else
		{
			if (shields_regening)
			{ 
				if (state_machine->adept->shield >= 65)
					shields_regening = false;
				agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, furthest_point);
			}
			else
			{
				agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, adept_scout_shade);
			}
		}*/


		if (agent->Observation()->GetGameLoop() > state_machine->frame_shade_used + 225)
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::EFFECT_ADEPTPHASESHIFT, shade_target);
		}
		/*else if ((state_machine->shade != NULL || Utility::DangerLevelAt(state_machine->adept, Utility::PointBetween(state_machine->adept->pos, adept_scout_shade, 1), agent->Observation()) > 0) && Distance2D(state_machine->adept->pos, adept_scout_runaway) > 1)
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, adept_scout_runaway);
		}
		else if ((state_machine->shade == NULL || Utility::DangerLevel(state_machine->adept, agent->Observation()) == 0) && Distance2D(state_machine->adept->pos, adept_scout_shade) > 1)
		{
			agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::MOVE_MOVE, adept_scout_shade);
		}*/

		if (state_machine->shade != NULL)
		{
			if (agent->Observation()->GetGameLoop() > state_machine->frame_shade_used + 150)
			{
				agent->Actions()->UnitCommand(state_machine->adept, ABILITY_ID::CANCEL_ADEPTPHASESHIFT);
				UpdateShadeTarget();
			}
			else
			{
				if (Distance2D(state_machine->shade->pos, shade_target) < 1)
				{
					if (shade_target == adept_scout_nat_path[0])
						shade_target = adept_scout_nat_path[1];
					else if (shade_target == adept_scout_ramptop)
						shade_target = adept_scout_base_spots[base_spots_index];
				}

				agent->Actions()->UnitCommand(state_machine->shade, ABILITY_ID::MOVE_MOVE, shade_target);
			}
		}
	}

	void AdeptBaseDefenseTerranScoutBase::EnterState()
	{
		return;
	}

	void AdeptBaseDefenseTerranScoutBase::ExitState()
	{
		return;
	}

	State* AdeptBaseDefenseTerranScoutBase::TestTransitions()
	{
		return NULL;
	}

	std::string AdeptBaseDefenseTerranScoutBase::toString()
	{
		std::string str = "scout base ";
		if (shade_target == adept_scout_nat_path[0])
			str += "natural 1";
		else if (shade_target == adept_scout_nat_path[1])
			str += "natural 2";
		else if (shade_target == adept_scout_ramptop)
			str += "ramp";
		else
			str += "main " + std::to_string(base_spots_index + 1);
		return str;
	}

	void AdeptBaseDefenseTerranScoutBase::UpdateShadeTarget()
	{
		auto found = std::find(adept_scout_nat_path.begin(), adept_scout_nat_path.end(), shade_target);
		if (found != adept_scout_nat_path.end())
		{
			shade_target = adept_scout_ramptop;
		}
		else
		{
			base_spots_index++;
			if (base_spots_index >= adept_scout_base_spots.size())
				base_spots_index = 0;

			if (base_spots_index % 2 == 0)
			{
				shade_target = adept_scout_nat_path[0];
			}
			else
			{
				shade_target = adept_scout_ramptop;
			}
		}
	}

#pragma endregion


#pragma region StalkerBaseDefenseTerranDefendFront

	// TODO change for maps with natural ramp
	void StalkerBaseDefenseTerranDefendFront::TickState()
	{
		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::TERRAN_REAPER)))
			{
				state_machine->target = unit;
				break;
			}
			if (state_machine->target == NULL)
			{
				if (forward)
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, state_machine->front_of_base[1]);
					if (Distance2D(state_machine->stalker->pos, state_machine->front_of_base[1]) < 1)
						forward = false;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, state_machine->front_of_base[0]);
					if (Distance2D(state_machine->stalker->pos, state_machine->front_of_base[0]) < 1)
						forward = true;
				}
			}
		}
		else
		{
			if (state_machine->attack_status == false)
			{
				// TODO move infront of units based on distance away
				if (Distance2D(state_machine->target->pos, state_machine->stalker->pos) <= 6 && state_machine->stalker->weapon_cooldown == 0)
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::ATTACK_ATTACK, state_machine->target);
					state_machine->attack_status = true;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);

				}
			}
			else if (state_machine->stalker->weapon_cooldown > 0)
			{
				state_machine->attack_status = false;
			}
		}

	}

	void StalkerBaseDefenseTerranDefendFront::EnterState()
	{
		return;
	}

	void StalkerBaseDefenseTerranDefendFront::ExitState()
	{
		return;
	}

	State* StalkerBaseDefenseTerranDefendFront::TestTransitions()
	{
		Units gates = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::PROTOSS_GATEWAY));
		Units other_units = agent->Observation()->GetUnits(IsUnits({ UNIT_TYPEID::PROTOSS_ADEPT, UNIT_TYPEID::PROTOSS_STALKER }));
		if (other_units.size() > 1 || agent->scout_info_terran.first_rax_production != reaper || (gates.size() > 0 && gates[0]->orders.size() > 0 && gates[0]->orders[0].progress > .9))
			return new StalkerBaseDefenseTerranMoveAcross(agent, state_machine);
		return NULL;
	}

	std::string StalkerBaseDefenseTerranDefendFront::toString()
	{
		return "defend front";
	}

#pragma endregion

#pragma region StalkerBaseDefenseTerranMoveAcross

	void StalkerBaseDefenseTerranMoveAcross::TickState()
	{
		if (state_machine->target == NULL)
		{
			for (const auto &unit : agent->Observation()->GetUnits(Unit::Alliance::Enemy))
			{
				if (Distance2D(state_machine->stalker->pos, unit->pos) < 8)
				{
					state_machine->target = unit;
					break;
				}
			}
			if (state_machine->target == NULL)
			{
				agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, agent->locations->adept_scout_runaway);
			}
		}
		else
		{
			if (Distance2D(state_machine->stalker->pos, state_machine->target->pos) > 8)
			{
				state_machine->target = NULL;
				return;
			}

			if (state_machine->attack_status == false)
			{
				// TODO move infront of units based on distance away
				if (Distance2D(state_machine->target->pos, state_machine->stalker->pos) <= 4 && state_machine->stalker->weapon_cooldown == 0)
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::ATTACK_ATTACK, state_machine->target);
					state_machine->attack_status = true;
				}
				else
				{
					agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, state_machine->target->pos);

				}
			}
			else if (state_machine->stalker->weapon_cooldown > 0)
			{
				state_machine->attack_status = false;
			}
		}

	}

	void StalkerBaseDefenseTerranMoveAcross::EnterState()
	{
		return;
	}

	void StalkerBaseDefenseTerranMoveAcross::ExitState()
	{
		return;
	}

	State* StalkerBaseDefenseTerranMoveAcross::TestTransitions()
	{
		if (Distance2D(state_machine->stalker->pos, agent->locations->adept_scout_runaway) < 3)
			return new ScoutBaseDefenseTerranHarrassFront(agent, state_machine, agent->locations->adept_scout_shade, agent->locations->adept_scout_runaway);
		return NULL;
	}

	std::string StalkerBaseDefenseTerranMoveAcross::toString()
	{
		return "move across map";
	}

#pragma endregion

#pragma region ScoutBaseDefenseTerranHarrassFront

	void ScoutBaseDefenseTerranHarrassFront::TickState()
	{
		Point2D furthest_point = attack_pos;
		Units enemy_units = agent->Observation()->GetUnits(IsFightingUnit(Unit::Alliance::Enemy));
		for (const auto &unit : enemy_units)
		{
			double range = Utility::RealGroundRange(unit, state_machine->stalker) + 2;
			Point2D intersection = agent->locations->attack_path_line.GetPointFrom(unit->pos, range, false);
			if (intersection != Point2D(0, 0))
			{
				float dist = Distance2D(intersection, attack_pos);
				if (dist > Distance2D(furthest_point, attack_pos))
				{
					furthest_point = intersection;
				}
			}
		}

		agent->Debug()->DebugSphereOut(agent->ToPoint3D(furthest_point), .7, Color(255, 255, 0));

		const Unit* closest_unit = Utility::ClosestTo(agent->Observation()->GetUnits(Unit::Alliance::Enemy), state_machine->stalker->pos);

		if (state_machine->attack_status == true)
		{
			if (state_machine->stalker->weapon_cooldown > 0)
				state_machine->attack_status = false;
		}
		else if (state_machine->stalker->shield < 5 || state_machine->stalker->weapon_cooldown > 0)
		{
			if (state_machine->stalker->shield < 5)
				shields_regening = true;
			agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, furthest_point);
		}
		else if (shields_regening)
		{
			if (state_machine->stalker->shield >= 75)
				shields_regening = false;
			agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, furthest_point);
		}
		else if (Distance2D(closest_unit->pos, state_machine->stalker->pos) <= 6)
		{
			agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::ATTACK_ATTACK, closest_unit);
			state_machine->attack_status = true;
		}
		else
		{
			agent->Actions()->UnitCommand(state_machine->stalker, ABILITY_ID::MOVE_MOVE, closest_unit);
		}
	}

	void ScoutBaseDefenseTerranHarrassFront::EnterState()
	{
		return;
	}

	void ScoutBaseDefenseTerranHarrassFront::ExitState()
	{
		return;
	}

	State* ScoutBaseDefenseTerranHarrassFront::TestTransitions()
	{
		return NULL;
	}

	std::string ScoutBaseDefenseTerranHarrassFront::toString()
	{
		return "harass front";
	}

#pragma endregion


#pragma region BlinkStalkerAttackTerranMoveAcross

	void BlinkStalkerAttackTerranMoveAcross::TickState()
	{

		agent->Actions()->UnitCommand(state_machine->army_group->stalkers, ABILITY_ID::MOVE_MOVE, state_machine->consolidation_pos);
		agent->Actions()->UnitCommand(state_machine->army_group->warp_prisms[0], ABILITY_ID::MOVE_MOVE, state_machine->prism_consolidation_pos);
	}

	void BlinkStalkerAttackTerranMoveAcross::EnterState()
	{
		return;
	}

	void BlinkStalkerAttackTerranMoveAcross::ExitState()
	{
		return;
	}

	State* BlinkStalkerAttackTerranMoveAcross::TestTransitions()
	{
		bool gates_almost_ready = true;
		for (const auto &status : agent->warpgate_status)
		{
			if (status.second.frame_ready > agent->Observation()->GetGameLoop() + 45)
			{
				gates_almost_ready = false;
				break;
			}
		}
		if (gates_almost_ready && agent->Observation()->IsPathable(state_machine->army_group->warp_prisms[0]->pos))
		{
			agent->Actions()->UnitCommand(state_machine->army_group->warp_prisms[0], ABILITY_ID::MOVE_MOVE, state_machine->army_group->warp_prisms[0]->pos);
			return new BlinkStalkerAttackTerranWarpIn(agent, state_machine);
		}

		if (Distance2D(Utility::FurthestFrom(state_machine->army_group->stalkers, state_machine->consolidation_pos)->pos, state_machine->consolidation_pos) < 4 && 
			Distance2D(state_machine->army_group->warp_prisms[0]->pos, state_machine->prism_consolidation_pos) < 2)
		{
			return new BlinkStalkerAttackTerranConsolidate(agent, state_machine);
		}
		return NULL;
	}

	std::string BlinkStalkerAttackTerranMoveAcross::toString()
	{
		return "move across";
	}

#pragma endregion

#pragma region BlinkStalkerAttackTerranWarpIn

	void BlinkStalkerAttackTerranWarpIn::TickState()
	{
		if (warping_in == false && state_machine->army_group->warp_prisms[0]->unit_type == UNIT_TYPEID::PROTOSS_WARPPRISMPHASING)
		{
			// try to warp in
			Units gates = agent->Observation()->GetUnits(IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
			if (Utility::CanAfford(UNIT_TYPEID::PROTOSS_STALKER, gates.size(), agent->Observation()))
			{
				std::vector<Point2D> spots = agent->FindWarpInSpots(agent->Observation()->GetGameInfo().enemy_start_locations[0], gates.size());
				std::cout << "spots " << spots.size() << "\n";
				if (spots.size() >= gates.size())
				{
					for (int i = 0; i < gates.size(); i++)
					{
						std::cout << "warp in at " << spots[i].x << ", " << spots[i].y << "\n";
						agent->Actions()->UnitCommand(gates[i], ABILITY_ID::TRAINWARP_STALKER, spots[i]);
						agent->warpgate_status[gates[i]].used = true;
						agent->warpgate_status[gates[i]].frame_ready = agent->Observation()->GetGameLoop() + round(23 * 22.4);
					}

					warping_in = true;
					warp_in_time = agent->Observation()->GetGameLoop();
				}
			}
		}
	}

	void BlinkStalkerAttackTerranWarpIn::EnterState()
	{
		agent->Actions()->UnitCommand(state_machine->army_group->warp_prisms[0], ABILITY_ID::MORPH_WARPPRISMPHASINGMODE);
	}

	void BlinkStalkerAttackTerranWarpIn::ExitState()
	{
		agent->Actions()->UnitCommand(state_machine->army_group->warp_prisms[0], ABILITY_ID::MORPH_WARPPRISMTRANSPORTMODE);
	}

	State* BlinkStalkerAttackTerranWarpIn::TestTransitions()
	{
		if (warping_in && agent->Observation()->GetGameLoop() > warp_in_time + 90)
			return new BlinkStalkerAttackTerranMoveAcross(agent, state_machine);
		return NULL;
	}

	std::string BlinkStalkerAttackTerranWarpIn::toString()
	{
		return "warp in";
	}

#pragma endregion

#pragma region BlinkStalkerAttackTerranConsolidate

	void BlinkStalkerAttackTerranConsolidate::TickState()
	{
		
	}

	void BlinkStalkerAttackTerranConsolidate::EnterState()
	{

	}

	void BlinkStalkerAttackTerranConsolidate::ExitState()
	{

	}

	State* BlinkStalkerAttackTerranConsolidate::TestTransitions()
	{
		return NULL;
	}

	std::string BlinkStalkerAttackTerranConsolidate::toString()
	{
		return "consolidate";
	}

#pragma endregion


#pragma region OracleHarassStateMachine

	void OracleHarassStateMachine::AddOracle(const Unit* oracle)
	{
		oracles.push_back(oracle);
		time_last_attacked[oracle] = 0;
		has_attacked[oracle] = true;
		is_beam_active[oracle] = false;
		casting[oracle] = false;
		casting_energy[oracle] = 0;
	}

	void OracleHarassStateMachine::OnUnitDestroyedListener(const Unit* oracle)
	{
		auto found = std::find(oracles.begin(), oracles.end(), oracle);
		if (found != oracles.end())
		{
			int index = found - oracles.begin();
			oracles.erase(oracles.begin() + index);
			OracleHarassAttackMineralLine* state = dynamic_cast<OracleHarassAttackMineralLine*>(current_state);
			if (state != NULL)
			{
				state->lost_oracle = true;
			}
		}
	}

#pragma endregion

#pragma region AdeptBaseDefenseTerran

	void AdeptBaseDefenseTerran::OnUnitCreatedListener(const Unit* unit)
	{
		if (shade == NULL && unit->unit_type == UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT && Distance2D(unit->pos, adept->pos) < .5)
		{
			shade = unit;
			frame_shade_used = agent->Observation()->GetGameLoop();
		}
	}

	void AdeptBaseDefenseTerran::OnUnitDestroyedListener(const Unit* unit)
	{
		if (unit == target)
			target = NULL;
		else if (unit == shade)
			shade = NULL;
	}

#pragma endregion

#pragma region StalkerBaseDefenseTerran

	void StalkerBaseDefenseTerran::OnUnitDestroyedListener(const Unit* unit)
	{
		if (unit == target)
			target = NULL;
	}

#pragma endregion
}