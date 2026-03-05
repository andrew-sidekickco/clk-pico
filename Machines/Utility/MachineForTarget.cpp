//
//  MachineForTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MachineForTarget.hpp"

#include <algorithm>

// Sources for runtime options and machines.
#include "Machines/Acorn/Archimedes/Archimedes.hpp"
#include "Machines/Acorn/BBCMicro/BBCMicro.hpp"
#include "Machines/Acorn/Electron/Electron.hpp"
#include "Machines/Amiga/Amiga.hpp"
#include "Machines/AmstradCPC/AmstradCPC.hpp"
#include "Machines/Apple/AppleII/AppleII.hpp"
#include "Machines/Apple/AppleIIgs/AppleIIgs.hpp"
#include "Machines/Apple/Macintosh/Macintosh.hpp"
#include "Machines/Atari/2600/Atari2600.hpp"
#include "Machines/Atari/ST/AtariST.hpp"
#include "Machines/ColecoVision/ColecoVision.hpp"
#include "Machines/Commodore/Plus4/Plus4.hpp"
#include "Machines/Commodore/Vic-20/Vic20.hpp"
#include "Machines/Enterprise/Enterprise.hpp"
#include "Machines/MasterSystem/MasterSystem.hpp"
#include "Machines/MSX/MSX.hpp"
#include "Machines/Oric/Oric.hpp"
#include "Machines/PCCompatible/PCCompatible.hpp"
#include "Machines/Sinclair/ZX8081/ZX8081.hpp"
#include "Machines/Sinclair/ZXSpectrum/ZXSpectrum.hpp"

// Sources for construction options.
#include "Analyser/Static/Acorn/Target.hpp"
#include "Analyser/Static/Amiga/Target.hpp"
#include "Analyser/Static/AmstradCPC/Target.hpp"
#include "Analyser/Static/AppleII/Target.hpp"
#include "Analyser/Static/AppleIIgs/Target.hpp"
#include "Analyser/Static/Atari2600/Target.hpp"
#include "Analyser/Static/AtariST/Target.hpp"
#include "Analyser/Static/Commodore/Target.hpp"
#include "Analyser/Static/Enterprise/Target.hpp"
#include "Analyser/Static/Macintosh/Target.hpp"
#include "Analyser/Static/MSX/Target.hpp"
#include "Analyser/Static/Oric/Target.hpp"
#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Analyser/Static/Sega/Target.hpp"
#include "Analyser/Static/ZX8081/Target.hpp"
#include "Analyser/Static/ZXSpectrum/Target.hpp"

#include "Analyser/Dynamic/MultiMachine/MultiMachine.hpp"
#include "TypedDynamicMachine.hpp"

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTarget(
	const Analyser::Static::Target *const target,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Machine::Error &error
) {
	error = Machine::Error::None;
	std::unique_ptr<Machine::DynamicMachine> machine;

	try {

#define BindD(name, m)	\
	case Analyser::Machine::m: \
		machine = std::make_unique<Machine::TypedDynamicMachine<::name::Machine>>(	\
			name::Machine::m(target, rom_fetcher)	\
		);		\
	break;
#define Bind(m)	BindD(m, m)
		switch(target->machine) {
			Bind(Electron)
			default:
				error = Machine::Error::UnknownMachine;
			return nullptr;
		}
#undef Bind
#undef BindD

	} catch(ROMMachine::Error construction_error) {
		switch(construction_error) {
			case ROMMachine::Error::MissingROMs:
				error = Machine::Error::MissingROM;
			break;
			default:
				error = Machine::Error::UnknownError;
			break;
		}
	}

	return machine;
}

std::unique_ptr<Machine::DynamicMachine> Machine::MachineForTargets(
	const Analyser::Static::TargetList &targets,
	const ROMMachine::ROMFetcher &rom_fetcher,
	Error &error
) {
	// Zero targets implies no machine.
	if(targets.empty()) {
		error = Error::NoTargets;
		return nullptr;
	}

	// If there's more than one target, get all the machines and combine them into a multimachine.
	if(targets.size() > 1) {
		std::vector<std::unique_ptr<Machine::DynamicMachine>> machines;
		for(const auto &target: targets) {
			machines.emplace_back(MachineForTarget(target.get(), rom_fetcher, error));

			// Exit early if any errors have occurred.
			if(error != Error::None) {
				return nullptr;
			}
		}

		// If a multimachine would just instantly collapse the list to a single machine, do
		// so without the ongoing baggage of a multimachine.
		if(Analyser::Dynamic::MultiMachine::would_collapse(machines)) {
			return std::move(machines.front());
		} else {
			return std::make_unique<Analyser::Dynamic::MultiMachine>(std::move(machines));
		}
	}

	// There's definitely exactly one target.
	return MachineForTarget(targets.front().get(), rom_fetcher, error);
}

std::string Machine::ShortNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::Electron:		return "Electron";
		default:	return "";
	}
}

std::string Machine::LongNameForTargetMachine(const Analyser::Machine machine) {
	switch(machine) {
		case Analyser::Machine::Electron:		return "Acorn Electron";
		default:	return "";
	}
}

std::vector<std::string> Machine::AllMachines(const Type type, const bool long_names) {
	std::vector<std::string> result;
	const auto add_name = [&](const Analyser::Machine machine) {
		result.push_back(
			long_names ? LongNameForTargetMachine(machine) : ShortNameForTargetMachine(machine)
		);
	};

	if(type == Type::Any || type == Type::RequiresMedia) {
		add_name(Analyser::Machine::Atari2600);
		add_name(Analyser::Machine::ColecoVision);
		add_name(Analyser::Machine::MasterSystem);
	}

	if(type == Type::Any || type == Type::DoesntRequireMedia) {
		add_name(Analyser::Machine::Electron);
	}

	return result;
}

std::map<std::string, std::unique_ptr<Reflection::Struct>> Machine::AllOptionsByMachineName() {
	std::map<std::string, std::unique_ptr<Reflection::Struct>> options;

#define Emplace(machine, class)														\
	options.emplace(																\
		LongNameForTargetMachine(Analyser::Machine::machine),						\
		std::make_unique<class::Options>(Configurable::OptionsType::UserFriendly)	\
	)

	Emplace(Electron, Electron::Machine);
#undef Emplace

	return options;
}

std::map<std::string, std::unique_ptr<Analyser::Static::Target>> Machine::TargetsByMachineName(
	const bool meaningful_without_media_only
) {
	std::map<std::string, std::unique_ptr<Analyser::Static::Target>> options;

#define AddMapped(Name, TargetNamespace)								\
	options.emplace(													\
		LongNameForTargetMachine(Analyser::Machine::Name),				\
		std::make_unique<Analyser::Static::TargetNamespace::Target>()	\
	);
#define Add(Name)	AddMapped(Name, Name)

	Add(Amiga);
	Add(AmstradCPC);
	Add(AppleII);
	Add(AppleIIgs);
	options.emplace(
		LongNameForTargetMachine(Analyser::Machine::Archimedes),
		std::make_unique<Analyser::Static::Acorn::ArchimedesTarget>()
	);
	Add(AtariST);
	options.emplace(
		LongNameForTargetMachine(Analyser::Machine::BBCMicro),
		std::make_unique<Analyser::Static::Acorn::BBCMicroTarget>()
	);
	options.emplace(
		LongNameForTargetMachine(Analyser::Machine::Electron),
		std::make_unique<Analyser::Static::Acorn::ElectronTarget>()
	);
	Add(Enterprise);
	Add(Macintosh);
	Add(MSX);
	Add(Oric);
	options.emplace(
		LongNameForTargetMachine(Analyser::Machine::Plus4),
		std::make_unique<Analyser::Static::Commodore::Plus4Target>()
	);
	Add(PCCompatible);
	options.emplace(
		LongNameForTargetMachine(Analyser::Machine::Vic20),
		std::make_unique<Analyser::Static::Commodore::Vic20Target>()
	);
	Add(ZX8081);
	Add(ZXSpectrum);

	if(!meaningful_without_media_only) {
		Add(Atari2600);
		options.emplace(
			LongNameForTargetMachine(Analyser::Machine::ColecoVision),
			std::make_unique<Analyser::Static::Target>(Analyser::Machine::ColecoVision)
		);
		AddMapped(MasterSystem, Sega);
	}

#undef Add
#undef AddMapped

	return options;
}
