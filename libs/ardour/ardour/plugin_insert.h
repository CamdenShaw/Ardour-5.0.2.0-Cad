/*
    Copyright (C) 2000,2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_plugin_insert_h__
#define __ardour_plugin_insert_h__

#include <vector>
#include <string>

#include <boost/weak_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/libardour_visibility.h"
#include "ardour/chan_mapping.h"
#include "ardour/fixed_delay.h"
#include "ardour/io.h"
#include "ardour/types.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/plugin.h"
#include "ardour/processor.h"
#include "ardour/sidechain.h"
#include "ardour/automation_control.h"

class XMLNode;

namespace ARDOUR {

class Session;
class Route;
class Plugin;

/** Plugin inserts: send data through a plugin
 */
class LIBARDOUR_API PluginInsert : public Processor
{
  public:
	PluginInsert (Session&, boost::shared_ptr<Plugin> = boost::shared_ptr<Plugin>());
	~PluginInsert ();

	static const std::string port_automation_node_name;

	XMLNode& state(bool);
	XMLNode& get_state(void);
	int set_state(const XMLNode&, int version);
	void update_id (PBD::ID);
	void set_state_dir (const std::string& d = "");

	void run (BufferSet& in, framepos_t start_frame, framepos_t end_frame, double speed, pframes_t nframes, bool);
	void silence (framecnt_t nframes, framepos_t start_frame);

	void activate ();
	void deactivate ();
	void flush ();

	void enable (bool yn);
	bool enabled () const;
	bool bypassable () const;

	bool reset_parameters_to_default ();
	bool can_reset_all_parameters ();

	int set_block_size (pframes_t nframes);

	ChanMapping input_map (uint32_t num) const {
		if (num < _in_map.size()) {
			return _in_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	ChanMapping output_map (uint32_t num) const {
		if (num < _out_map.size()) {
			return _out_map.find (num)->second;
		} else {
			return ChanMapping ();
		}
	}

	ChanMapping thru_map () const {
		return _thru_map;
	}

	bool pre_seed (const ChanCount&, const ChanCount&, const ChanMapping&, const ChanMapping&, const ChanMapping&);

	ChanMapping input_map () const; ///< combined (all instances) input map
	ChanMapping no_sc_input_map () const; ///< combined (all instances) input map w/o sidechain sinks
	ChanMapping output_map () const; ///< combined (all instances) output map
	bool has_midi_bypass () const;
	bool has_midi_thru () const;
	bool inplace () const { return ! _no_inplace; }

#ifdef MIXBUS
	bool is_channelstrip () const;
#endif

	void set_input_map (uint32_t, ChanMapping);
	void set_output_map (uint32_t, ChanMapping);
	void set_thru_map (ChanMapping);
	bool reset_map (bool emit = true);
	bool sanitize_maps ();
	bool check_inplace ();

	// these are ports visible on the outside
	ChanCount output_streams() const;
	ChanCount input_streams() const;
	ChanCount internal_streams() const; // with side-chain

	// actual ports of all plugins.
	// n * natural_i/o or result of reconfigurable i/o
	ChanCount internal_output_streams() const;
	ChanCount internal_input_streams() const;

	// a single plugin's internal i/o
	ChanCount natural_output_streams() const;
	ChanCount natural_input_streams() const;

	/** plugin ports marked as sidechain */
	ChanCount sidechain_input_pins() const;

	/** Plugin-Insert IO sidechain ports */
	ChanCount sidechain_input_ports() const {
		if (_sidechain) {
			return _sidechain->input ()->n_ports ();
		} else {
			return ChanCount ();
		}
	}

	const ChanCount& required_buffers () const { return _required_buffers; }
	const ChanCount& preset_out () const { return _preset_out; }

	// allow to override output_streams(), implies "Custom Mode"

	// only the owning route may call these (with process lock held)
	// route is not a friend class, it owns us
	bool set_count      (uint32_t num);
	void set_sinks      (const ChanCount&); // reconfigurable I/O ONLY
	void set_outputs    (const ChanCount&);
	void set_strict_io  (bool b);
	void set_custom_cfg (bool b);
	bool set_preset_out (const ChanCount&);
	bool add_sidechain  (uint32_t n_audio = 1, uint32_t n_midi = 0);
	bool del_sidechain ();
	void set_sidechain_latency (uint32_t, uint32_t);
	boost::shared_ptr<SideChain> sidechain () const { return _sidechain; }
	// end C++ class slavery!

	uint32_t get_count  () const { return _plugins.size(); }
	bool     strict_io  () const { return _strict_io; }
	bool     custom_cfg () const { return _custom_cfg; }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	bool has_no_inputs() const;
	bool has_no_audio_inputs() const;
	bool needs_midi_input() const;
	bool has_output_presets (
			ChanCount in = ChanCount (DataType::MIDI, 1),
			ChanCount out = ChanCount (DataType::AUDIO, 2)
			);

	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	void monitoring_changed ();

	bool load_preset (Plugin::PresetRecord);

	/** A control that manipulates a plugin parameter (control port). */
	struct PluginControl : public AutomationControl
	{
		PluginControl (PluginInsert*                     p,
		               const Evoral::Parameter&          param,
		               const ParameterDescriptor&        desc,
		               boost::shared_ptr<AutomationList> list=boost::shared_ptr<AutomationList>());

		double get_value (void) const;
		void catch_up_with_external_value (double val);
		XMLNode& get_state();

	private:
		PluginInsert* _plugin;
		void actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	};

	/** A control that manipulates a plugin property (message). */
	struct PluginPropertyControl : public AutomationControl
	{
		PluginPropertyControl (PluginInsert*                     p,
		                       const Evoral::Parameter&          param,
		                       const ParameterDescriptor&        desc,
		                       boost::shared_ptr<AutomationList> list=boost::shared_ptr<AutomationList>());

		double get_value (void) const;
		XMLNode& get_state();
        protected:
		void actually_set_value (double value, PBD::Controllable::GroupControlDisposition);

	private:
		PluginInsert* _plugin;
		Variant       _value;
	};

	boost::shared_ptr<Plugin> plugin(uint32_t num=0) const {
		if (num < _plugins.size()) {
			return _plugins[num];
		} else {
			return _plugins[0]; // we always have one
		}
	}

	framecnt_t plugin_latency () const;

	bool has_sidechain () const {
		return _sidechain ? true : false;
	}

	boost::shared_ptr<IO> sidechain_input () const {
		if (_sidechain) {
			return _sidechain->input ();
		}
		return boost::shared_ptr<IO> ();
	}

	PluginType type ();

	std::string describe_parameter (Evoral::Parameter param);

	framecnt_t signal_latency () const;

	boost::shared_ptr<Plugin> get_impulse_analysis_plugin();

	void collect_signal_for_analysis (framecnt_t nframes);

	bool strict_io_configured () const {
		return _match.strict_io;
	}

	bool splitting () const {
		return _match.method == Split;
	}

	void configured_io (ChanCount &in, ChanCount &out) const {
		in = _configured_in;
		out = _configured_out;
	}

	PBD::Signal2<void,BufferSet*, BufferSet*> AnalysisDataGathered;
	PBD::Signal0<void> PluginIoReConfigure;
	PBD::Signal0<void> PluginMapChanged;
	PBD::Signal0<void> PluginConfigChanged;

	/** Enumeration of the ways in which we can match our insert's
	 *  IO to that of the plugin(s).
	 */
	enum MatchingMethod {
		Impossible,  ///< we can't
		Delegate,    ///< we are delegating to the plugin, and it can handle it
		NoInputs,    ///< plugin has no inputs, so anything goes
		ExactMatch,  ///< our insert's inputs are the same as the plugin's
		Replicate,   ///< we have multiple instances of the plugin
		Split,       ///< we copy one of our insert's inputs to multiple plugin inputs
		Hide,        ///< we `hide' some of the plugin's inputs by feeding them silence
	};

	/** Description of how we can match our plugin's IO to our own insert IO */
	struct Match {
		Match () : method (Impossible), plugins (0), strict_io (false), custom_cfg (false) {}
		Match (MatchingMethod m, int32_t p,
				bool strict = false, bool custom = false, ChanCount h = ChanCount ())
			: method (m), plugins (p), hide (h), strict_io (strict), custom_cfg (custom) {}

		MatchingMethod method; ///< method to employ
		int32_t plugins;       ///< number of copies of the plugin that we need
		ChanCount hide;        ///< number of channels to hide
		bool strict_io;        ///< force in == out
		bool custom_cfg;       ///< custom config (if not strict)
	};

  private:
	/* disallow copy construction */
	PluginInsert (const PluginInsert&);

	void parameter_changed_externally (uint32_t, float);

	void set_parameter (Evoral::Parameter param, float val);

	float default_parameter_value (const Evoral::Parameter& param);

	typedef std::vector<boost::shared_ptr<Plugin> > Plugins;
	Plugins _plugins;

	boost::shared_ptr<SideChain> _sidechain;
	uint32_t _sc_playback_latency;
	uint32_t _sc_capture_latency;
	uint32_t _plugin_signal_latency;

	boost::weak_ptr<Plugin> _impulseAnalysisPlugin;

	framecnt_t _signal_analysis_collected_nframes;
	framecnt_t _signal_analysis_collect_nframes_max;

	BufferSet _signal_analysis_inputs;
	BufferSet _signal_analysis_outputs;

	FixedDelay _delaybuffers;

	ChanCount _configured_in;
	ChanCount _configured_internal; // with side-chain
	ChanCount _configured_out;
	ChanCount _custom_out;
	ChanCount _custom_sinks;
	ChanCount _preset_out;
	ChanCount _cached_sidechain_pins;
	ChanCount _required_buffers;

	bool _configured;
	bool _no_inplace;
	bool _strict_io;
	bool _custom_cfg;
	bool _maps_from_state;
	bool _mapping_changed;

	Match private_can_support_io_configuration (ChanCount const &, ChanCount &) const;
	Match internal_can_support_io_configuration (ChanCount const &, ChanCount &) const;
	Match automatic_can_support_io_configuration (ChanCount const &, ChanCount &) const;

	/** details of the match currently being used */
	Match _match;

	typedef std::map <uint32_t, ARDOUR::ChanMapping> PinMappings;
	PinMappings _in_map;
	PinMappings _out_map;
	ChanMapping _thru_map; // out-idx <=  in-idx

	void automation_run (BufferSet& bufs, framepos_t start, framepos_t end, double speed, pframes_t nframes);
	void connect_and_run (BufferSet& bufs, framepos_t start, framecnt_t end, double speed, pframes_t nframes, framecnt_t offset, bool with_auto);
	void bypass (BufferSet& bufs, pframes_t nframes);
	void inplace_silence_unconnected (BufferSet&, const PinMappings&, framecnt_t nframes, framecnt_t offset) const;

	void create_automatable_parameters ();
	void control_list_automation_state_changed (Evoral::Parameter, AutoState);
	void set_parameter_state_2X (const XMLNode& node, int version);
	void set_control_ids (const XMLNode&, int version);

	void enable_changed ();
	void bypassable_changed ();

	boost::shared_ptr<Plugin> plugin_factory (boost::shared_ptr<Plugin>);
	void add_plugin (boost::shared_ptr<Plugin>);

	void start_touch (uint32_t param_id);
	void end_touch (uint32_t param_id);

	void latency_changed ();
	bool _latency_changed;
	uint32_t _bypass_port;
};

} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::PluginInsert::Match& m);

#endif /* __ardour_plugin_insert_h__ */