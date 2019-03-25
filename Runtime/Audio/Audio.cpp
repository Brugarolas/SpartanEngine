/*
Copyright(c) 2016-2019 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES =============================
#include "Audio.h"
#include <fmod.hpp>
#include <fmod_errors.h>
#include <sstream>
#include "../Core/Engine.h"
#include "../Core/EventSystem.h"
#include "../Core/Settings.h"
#include "../Profiling/Profiler.h"
#include "../World/Components/Transform.h"
#include "../Core/Context.h"
//========================================

//= NAMESPACES ======
using namespace std;
using namespace FMOD;
//===================

namespace Directus
{
	Audio::Audio(Context* context) : ISubsystem(context)
	{
		m_system_fmod		= nullptr;
		m_max_channels		= 32;
		m_distance_entity	= 1.0f;
		m_listener			= nullptr;
		m_profiler			= m_context->GetSubsystem<Profiler>().get();

		// Create FMOD instance
		m_result_fmod = System_Create(&m_system_fmod);
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		// Check FMOD version
		unsigned int version;
		m_result_fmod = m_system_fmod->getVersion(&version);
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		if (version < FMOD_VERSION)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		// Make sure there is a sound card devices on the machine
		auto driver_count = 0;
		m_result_fmod = m_system_fmod->getNumDrivers(&driver_count);
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		// Initialize FMOD
		m_result_fmod = m_system_fmod->init(m_max_channels, FMOD_INIT_NORMAL, nullptr);
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		// Set 3D settings
		m_result_fmod = m_system_fmod->set3DSettings(1.0, m_distance_entity, 0.0f);
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		m_initialized = true;

		// Get version
		stringstream ss;
		ss << hex << version;
		const auto major	= ss.str().erase(1, 4);
		const auto minor	= ss.str().erase(0, 1).erase(2, 2);
		const auto rev		= ss.str().erase(0, 3);
		Settings::Get().m_versionFMOD = major + "." + minor + "." + rev;

		// Subscribe to events
		SUBSCRIBE_TO_EVENT(Event_World_Unload, [this](Variant) { m_listener = nullptr; });
	}

	Audio::~Audio()
	{
		// Unsubscribe from events
		UNSUBSCRIBE_FROM_EVENT(Event_World_Unload, [this](Variant) { m_listener = nullptr; });

		if (!m_system_fmod)
			return;

		// Close FMOD
		m_result_fmod = m_system_fmod->close();
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		// Release FMOD
		m_result_fmod = m_system_fmod->release();
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
		}
	}

	void Audio::Tick()
	{
		// Don't play audio if the engine is not in game mode
		if (!Engine::EngineMode_IsSet(Engine_Game))
			return;

		if (!m_initialized)
			return;

		TIME_BLOCK_START_CPU(m_profiler);

		// Update FMOD
		m_result_fmod = m_system_fmod->update();
		if (m_result_fmod != FMOD_OK)
		{
			LogErrorFmod(m_result_fmod);
			return;
		}

		//= 3D Attributes =============================================
		if (m_listener)
		{
			auto position = m_listener->GetPosition();
			auto velocity = Math::Vector3::Zero;
			auto forward = m_listener->GetForward();
			auto up = m_listener->GetUp();

			// Set 3D attributes
			m_result_fmod = m_system_fmod->set3DListenerAttributes(
				0, 
				reinterpret_cast<FMOD_VECTOR*>(&position), 
				reinterpret_cast<FMOD_VECTOR*>(&velocity), 
				reinterpret_cast<FMOD_VECTOR*>(&forward), 
				reinterpret_cast<FMOD_VECTOR*>(&up)
			);
			if (m_result_fmod != FMOD_OK)
			{
				LogErrorFmod(m_result_fmod);
				return;
			}
		}
		//=============================================================

		TIME_BLOCK_END(m_profiler);
	}

	void Audio::SetListenerTransform(Transform* transform)
	{
		m_listener = transform;
	}

	void Audio::LogErrorFmod(int error) const
	{
		LOG_ERROR("Audio::FMOD: " + string(FMOD_ErrorString(static_cast<FMOD_RESULT>(error))));
	}
}