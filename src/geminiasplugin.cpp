/**
 * @file geminiasplugin.cpp  Plug-in wrapper for the Gemini Sproutlet.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "cfgoptions.h"
#include "cfgoptions_helper.h"
#include "sproutletplugin.h"
#include "mobiletwinned.h"
#include "sproutletappserver.h"
#include "log.h"

class GeminiPlugin : public SproutletPlugin
{
public:
  GeminiPlugin();
  ~GeminiPlugin();

  bool load(struct options& opt, std::list<Sproutlet*>& sproutlets);
  void unload();

private:
  MobileTwinnedAppServer* _gemini;
  SproutletAppServerShim* _gemini_sproutlet;
};

/// Export the plug-in using the magic symbol "sproutlet_plugin"
extern "C" {
GeminiPlugin sproutlet_plugin;
}


GeminiPlugin::GeminiPlugin() :
  _gemini(NULL),
  _gemini_sproutlet(NULL)
{
}

GeminiPlugin::~GeminiPlugin()
{
}

/// Loads the Gemini plug-in, returning the supported Sproutlets.
bool GeminiPlugin::load(struct options& opt, std::list<Sproutlet*>& sproutlets)
{
  bool plugin_loaded = true;

  std::string gemini_prefix = "gemini";
  int gemini_port = 0;
  std::string gemini_uri = "";
  bool gemini_enabled = true;
  std::string plugin_name = "gemini-as";

  std::map<std::string, std::multimap<std::string, std::string>>::iterator
    gemini_it = opt.plugin_options.find(plugin_name);

  if (gemini_it == opt.plugin_options.end())
  {
    TRC_STATUS("Gemini options not specified on Sprout command. Gemini disabled.");
    gemini_enabled = false;
  }
  else
  {
    TRC_DEBUG("Got Gemini options map");
    std::multimap<std::string, std::string>& gemini_opts = gemini_it->second;

    set_plugin_opt_int(gemini_opts,
                       "gemini",
                       "gemini-as",
                       true,
                       gemini_port,
                       gemini_enabled);

    if (gemini_port < 0)
    {
      TRC_STATUS("Gemini port set to a value of less than zero (%d). Disabling gemini.",
                 gemini_port);
      gemini_enabled = false;
    }

    set_plugin_opt_str(gemini_opts,
                       "gemini_prefix",
                       "gemini-as",
                       false,
                       gemini_prefix,
                       gemini_enabled);

    // Given the prefix, set the default uri
    gemini_uri = "sip:" + gemini_prefix + "." + opt.sprout_hostname + ";transport=TCP";

    set_plugin_opt_str(gemini_opts,
                       "gemini_uri",
                       "gemini-as",
                       false,
                       gemini_uri,
                       gemini_enabled);
  }

  if (gemini_enabled)
  {
    TRC_STATUS("Gemini plugin enabled");

    SNMP::SuccessFailCountByRequestTypeTable* incoming_sip_transactions = SNMP::SuccessFailCountByRequestTypeTable::create("gemini_as_incoming_sip_transactions",
                                                                                                                           "1.2.826.0.1.1578918.9.11.1");
    SNMP::SuccessFailCountByRequestTypeTable* outgoing_sip_transactions = SNMP::SuccessFailCountByRequestTypeTable::create("gemini_as_outgoing_sip_transactions",
                                                                                                                           "1.2.826.0.1.1578918.9.11.2");
    // Create the Sproutlet.
    _gemini = new MobileTwinnedAppServer(gemini_prefix);
    _gemini_sproutlet = new SproutletAppServerShim(_gemini,
                                                   gemini_port,
                                                   gemini_uri,
                                                   incoming_sip_transactions,
                                                   outgoing_sip_transactions);

    sproutlets.push_back(_gemini_sproutlet);
  }

  return plugin_loaded;
}

/// Unloads the Gemini plug-in.
void GeminiPlugin::unload()
{
  delete _gemini_sproutlet;
  delete _gemini;
}
