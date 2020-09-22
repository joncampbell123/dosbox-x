/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/* This module: 3/2002
 * Author: Markus Nentwig, nentwig@users.sourceforge.net
 */

#define PrintErrorMessage -1

#include "fluidsynth_priv.h"

#ifdef LADSPA
#include <assert.h>

/* Dynamic library functions */
#include <dlfcn.h>

#include "fluid_ladspa.h"
#include "fluid_synth.h"

/* Logging to stdout. */
//#define L(x) x;printf("\n");
#define L(x);

fluid_LADSPA_FxUnit_t* new_fluid_LADSPA_FxUnit(fluid_synth_t* synth){
  fluid_LADSPA_FxUnit_t* FxUnit=FLUID_NEW(fluid_LADSPA_FxUnit_t);
  assert(FxUnit);
  assert(synth);
  /* The default state is 'bypassed'. The Fx unit has to be turned on explicitly by the user. */
  /* Those settings have to be done in order to allow fluid_LADSPA_clean. */
  FxUnit->Bypass=fluid_LADSPA_Bypassed;
  FxUnit->NumberNodes=0;
  FxUnit->NumberPlugins=0;
  FxUnit->NumberLibs=0;
  FxUnit->NumberCommands=0;
  FxUnit->NumberUserControlNodes=0;
  FxUnit->synth=synth;
  pthread_cond_init(&FxUnit->cond,NULL);
  return FxUnit;
};

/* Purpose:
 * Creates the system nodes to get data into and out of the Fx unit.
 */
void fluid_LADSPA_CreateSystemNodes(fluid_LADSPA_FxUnit_t* FxUnit){
  char str[99];
  int nr_input_nodes;
  int nr_fx_input_nodes;
  int nr_output_nodes;
  int temp;
  int i;

  /* Retrieve the number of synth / audio out / Fx send nodes */
  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.audio-groups", &temp));
  nr_input_nodes=(int) temp;
  printf("%i audio groups\n", nr_input_nodes);

  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.audio-channels", &temp));
  nr_output_nodes=temp;

  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.effects-channels", &temp));
  nr_fx_input_nodes=temp;

  /* Create regular input nodes (associated with audio groups) */
  for (i=0; i < nr_input_nodes; i++){
      sprintf(str, "in%i_L",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_source);
      sprintf(str, "in%i_R",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_source);
  };

  /* Create effects send nodes (for example reverb, chorus send) */
  for (i=0; i < nr_fx_input_nodes; i++){
      sprintf(str, "send%i_L",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_source);
      sprintf(str, "send%i_R",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_source);
  };

  /* Create output nodes (usually towards the sound card) */
  for (i=0; i < nr_input_nodes; i++){
      sprintf(str, "out%i_L",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_sink);
      sprintf(str, "out%i_R",(i+1));
      fluid_LADSPA_CreateNode(FxUnit, str, fluid_LADSPA_node_is_audio | fluid_LADSPA_node_is_sink);
  };
};

/* Purpose:
 * Creates predeclared nodes for control of the Fx unit during operation.
 */
void fluid_LADSPA_CreateUserControlNodes(fluid_LADSPA_FxUnit_t* FxUnit){
  int i;
  fluid_LADSPA_Node_t* CurrentNode;

  for (i=0; i<FxUnit->NumberUserControlNodes; i++){
    CurrentNode=fluid_LADSPA_CreateNode(FxUnit,FxUnit->UserControlNodeNames[i],fluid_LADSPA_node_is_control);
    assert(CurrentNode);
    CurrentNode->buf[0]=FxUnit->UserControlNodeValues[i];
    CurrentNode->InCount++; /* The constant counts as input */
    CurrentNode->flags=fluid_LADSPA_node_is_source | fluid_LADSPA_node_is_user_ctrl; /* It is a user control node */
  };
};

/* Purpose:
 * Returns the pointer to the shared library loaded using 'LibraryFilename' (if it has been loaded).
 * Return NULL otherwise.
 * If not: Clear Fx and abort.
 */
void * fluid_LADSPA_RetrieveSharedLibrary(fluid_LADSPA_FxUnit_t* FxUnit, char * LibraryFilename){
  void * CurrentLib=NULL;
  int LibCount;
  for (LibCount=0; LibCount<FxUnit->NumberLibs; LibCount++){
    assert(FxUnit->ppvPluginLibNames[LibCount]);
    if (FLUID_STRCMP(FxUnit->ppvPluginLibNames[LibCount],LibraryFilename)==0){
      CurrentLib=FxUnit->ppvPluginLibs[LibCount];
    };
  };
  return CurrentLib;
};

/* Purpose:
 * Loads a shared LADSPA library.
 * Return NULL, if failed.
 * TODO: use LADSPA_PATH
 */
void * fluid_LADSPA_LoadSharedLibrary(fluid_LADSPA_FxUnit_t* FxUnit, char * LibraryFilename){
  void * LoadedLib;
  assert(LibraryFilename);
  LoadedLib=dlopen(LibraryFilename,RTLD_NOW);
  if (!LoadedLib){
    return NULL;
  };
  FxUnit->ppvPluginLibs[FxUnit->NumberLibs]=LoadedLib;
  FxUnit->ppvPluginLibNames[FxUnit->NumberLibs]=FLUID_STRDUP(LibraryFilename);
  FxUnit->NumberLibs++;
  return LoadedLib;
};

/* Purpose:
 * Retrieves a descriptor to the plugin labeled 'PluginLabel' from the shared library.
 */
const LADSPA_Descriptor * fluid_LADSPA_Retrieve_Plugin_Descriptor(void * CurrentLib, char * PluginLabel){
  LADSPA_Descriptor_Function pfDescriptorFunction;
  unsigned long lPluginIndex=0;
  const LADSPA_Descriptor * psDescriptor;
  pfDescriptorFunction = (LADSPA_Descriptor_Function)dlsym(CurrentLib,"ladspa_descriptor");
  while (pfDescriptorFunction(lPluginIndex)){
    psDescriptor = pfDescriptorFunction(lPluginIndex);
    if (FLUID_STRCMP(psDescriptor->Label, PluginLabel) == 0){
      return psDescriptor;
    };
    lPluginIndex++;
  };
  return NULL;
};

/* Purpose:
 * Finds out, if 'PortName' starts with 'PluginPort'.
 * Spaces and underscore mean the same.
 * The comparison is not case sensitive.
 * The result distinguishes between a full match (strings are equal), and a partial match (PortName starts with Plugin_Port, but is longer).
 */

fluid_LADSPA_Stringmatch_t fluid_LADSPA_Check_SubString_Match(const char * Plugin_Port, const char * PortName){
  unsigned int CharCount;
  char a;
  char b;
  for(CharCount=0; CharCount<FLUID_STRLEN(Plugin_Port); CharCount++){
    a=PortName[CharCount];
    b=Plugin_Port[CharCount];
    if (a>='a' && a <='z'){a-=32;}
    if (b>='a' && b <='z'){b-=32;}
    if (a == ' '){a='_';};
    if (b == ' '){b='_';};
    if ( a != b){
      return fluid_LADSPA_NoMatch;
    };
  };
  if (FLUID_STRLEN(Plugin_Port) == FLUID_STRLEN(PortName)){
    return fluid_LADSPA_FullMatch;
  };
  return fluid_LADSPA_PartialMatch;

};

/* Purpose:
 * Load the plugins added with 'ladspa_add' and then start the Fx unit.
 */

int
fluid_LADSPA_handle_start(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out){
  fluid_LADSPA_FxUnit_t* FxUnit;
  int CommandLineCount;
  char * LibraryFilename;
  char * PluginLabel;
  char ** TokenSequence;
  void * CurrentLib;
  int NodeCount; //x
  int IngoingSignalCount; // Count signals going into LADSPA Fx section
  int OutgoingSignalCount; // Count signals going out of LADSPA Fx section
  int ReturnVal=FLUID_OK; /* If warnings occur, this is set to -1. */
  char * LADSPA_Path = getenv("LADSPA_PATH");

  L(fluid_ostream_printf(out,"ladspa_start: starting..."));
  assert(synth);
  FxUnit=synth->LADSPA_FxUnit; assert(FxUnit);

  /* When calling fluid_ladspastart, the Fx unit must be 'cleared' (no plugins, no libs, no nodes). Verify this here. */
  if (FxUnit->NumberPlugins || FxUnit->NumberLibs){
    fluid_ostream_printf(out, "***Error006***\n"
	     "Fx unit is currently in use!\n"
	     "Please run the ladspa_clear command before attempting to use ladspa_add!\n");
    /* In this case do _not_ clear the Fx unit. */
    return(PrintErrorMessage);
  };
  if (!FxUnit->NumberCommands){
    fluid_ostream_printf(out, "***Error007***\n"
	     "Refusing to start the Fx unit without any plugin.\n"
	     "Use ladspa_add first!\n");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };

  /* Create predefined nodes */
  L(fluid_ostream_printf(out,"ladspa_start: creating predefined nodes..."));
  fluid_LADSPA_CreateSystemNodes(FxUnit);

  /* Create predeclared nodes, that will allow to control the Fx unit during operation */
  L(fluid_ostream_printf(out,"ladspa_start: creating user control nodes..."));
  fluid_LADSPA_CreateUserControlNodes(FxUnit);

  L(fluid_ostream_printf(out,"ladspa_start: Processing command lines..."));
  for (CommandLineCount=0; CommandLineCount<FxUnit->NumberCommands; CommandLineCount++){
    int CurrentPlugin_PortConnected[FLUID_LADSPA_MaxTokens/3]; /* For example: 100 tokens corresponds to roughly 30 ports. */
    char LibFullPath[FLUID_LADSPA_MaxPathLength];
    const LADSPA_Descriptor * CurrentPluginDescriptor;
    LADSPA_Handle CurrentPlugin;
    unsigned long PortCount;
    int TokenCount=0;
    char * Search;
    int HasASlash=0;

    if (FxUnit->NumberPlugins>=FLUID_LADSPA_MaxPlugins){
    fluid_ostream_printf(out, "***Error003***\n"
	       "Too many plugins at the same time (%i).\n"
	       "Change FLUID_LADSPA_MaxPlugins!\n",
	       FxUnit->NumberPlugins);
      fluid_LADSPA_clear(FxUnit);
      return(PrintErrorMessage);
    };

    L(fluid_ostream_printf(out,"Processing plugin nr. %i",FxUnit->NumberPlugins));

    TokenSequence=FxUnit->LADSPA_Command_Sequence[CommandLineCount];
    assert(TokenSequence);

    /* Check, if the library is already loaded. If not, load. */
    LibraryFilename=TokenSequence[TokenCount++]; assert(LibraryFilename);

    L(fluid_ostream_printf(out,"Library name from ladspa_add: %s",LibraryFilename));
    /* A slash-free filename refers to the LADSPA_PATH directory. Add that path, if needed. */



    /* Determine, if the library filename contains a slash.
     * If no, try to retrieve the environment variable LADSPA_PATH.
     * If that fails, signal error.
     * Otherwise leave the filename as it is.
     */;

    /* Determine, if the library name is just the filename, or a path (including a slash) */
    Search=LibraryFilename;
    while (*Search != '\0') {
      if ((*Search)== '/'){
	HasASlash=1;
      };
      Search++;
    };

    if (!HasASlash){
      if (!LADSPA_Path){
    fluid_ostream_printf(out, "***Error018***\n"
		 "The library file name %s does not include a path.\n"
		 "The environment variable LADSPA_PATH is not set.\n"
		 "- Use an absolute path (i.e. /home/myself/mylib.so)\n"
		 "- For the current directory use ./mylib.so\n"
		 "- set the environment variable LADSPA_PATH (export LADSPA_PATH=/usr/lib/ladspa)\n"
		 "- depending on your shell, try 'setenv' instead of 'export'\n",
		 LibraryFilename);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      }; /* if no LADSPA_PATH */
      snprintf(LibFullPath,FLUID_LADSPA_MaxPathLength,"%s/%s",LADSPA_Path,LibraryFilename);
      /* If no slash in filename */
    } else {
	snprintf(LibFullPath,FLUID_LADSPA_MaxPathLength,"%s",LibraryFilename);
    };

    L(fluid_ostream_printf(out,"Full Library path name: %s",LibFullPath));

    CurrentLib=fluid_LADSPA_RetrieveSharedLibrary(FxUnit, LibFullPath);
    if (!CurrentLib){
      LADSPA_Descriptor_Function pfDescriptorFunction;

      L(fluid_ostream_printf(out,"Library %s not yet loaded. Loading.",LibFullPath));

      if (FxUnit->NumberLibs>=FLUID_LADSPA_MaxLibs){
    fluid_ostream_printf(out, "***Error004***\n"
		 "Too many libraries open (%i)\n"
		 "Change FLUID_LADSPA_MaxLibs",FxUnit->NumberPlugins);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };

      /* Load a LADSPA plugin library and store it for future use.*/
      CurrentLib=fluid_LADSPA_LoadSharedLibrary(FxUnit, LibFullPath);

      if (!CurrentLib){
    fluid_ostream_printf(out, "***Error008***\n"
		 "Failed to load plugin library %s.",
		 LibraryFilename);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };


      dlerror();
      pfDescriptorFunction = (LADSPA_Descriptor_Function)dlsym(CurrentLib,"ladspa_descriptor");
      if (!pfDescriptorFunction) {
	const char * pcError = dlerror();
	if (!pcError) {pcError="Huh?! No error from lib!";};
    fluid_ostream_printf(out, "***Error015***\n"
		 "Unable to find ladspa_descriptor() function in plugin library file \"%s\": %s.\n"
		 "Are you sure this is a LADSPA plugin file?\n",
		 LibraryFilename,
		 pcError);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };

      L(fluid_ostream_printf(out,"Library loaded."));
    };

    PluginLabel=TokenSequence[TokenCount++]; assert(PluginLabel);
    L(fluid_ostream_printf(out,"Plugin Label from ladspa_add: %s",PluginLabel));
    /* Retrieve a 'plugin descriptor' from the library. */
    L(fluid_ostream_printf(out,"Looking for the plugin labeled %s",PluginLabel));

    CurrentPluginDescriptor=fluid_LADSPA_Retrieve_Plugin_Descriptor(CurrentLib, PluginLabel);


    /* Check, that the plugin was actually found.*/
    if (CurrentPluginDescriptor==NULL){
    fluid_ostream_printf(out, "***Error016***\n"
	       "Unable to find the plugin labeled \"%s\" in plugin library file \"%s\".\n"
	       "Hint: run analyzeplugin %s from the command line to get a list of valid plugin labels.\n",
	       PluginLabel,
	       LibraryFilename,
	       LibraryFilename);
      fluid_LADSPA_clear(FxUnit);
      return(PrintErrorMessage);
    };

    /* Create an instance of the plugin type from the descriptor */
    L(fluid_ostream_printf(out,"instantiating plugin %s",PluginLabel));
    CurrentPlugin=CurrentPluginDescriptor
      ->instantiate(CurrentPluginDescriptor,44100); /* Sample rate hardcoded */
    assert(CurrentPlugin);

    /* The descriptor ("type of plugin") and the instance are stored for each plugin instantiation.
       If one plugin type is instantiated several times, they will have the same descriptor, only
       different instances.*/
    FxUnit->PluginDescriptorTable[FxUnit->NumberPlugins]=CurrentPluginDescriptor;
    FxUnit->PluginInstanceTable[FxUnit->NumberPlugins]=CurrentPlugin;

    /*
     *
     * Wire up the inputs and outputs
     *
     */

    /* List for checking, that each plugin port is exactly connected once */
    for (PortCount=0; PortCount<CurrentPluginDescriptor->PortCount; PortCount++){
      CurrentPlugin_PortConnected[PortCount]=0;
    };

    /* Note: There are three NULL tokens at the end. The last condition may be evaluated even if the first one already detects the end. */
    while (TokenSequence[TokenCount] && TokenSequence[TokenCount+1] && TokenSequence[TokenCount+2]){
      int CurrentPort_StringMatchCount;
      LADSPA_PortDescriptor CurrentPort_Descriptor;
      char * Plugin_Port=TokenSequence[TokenCount++];
      char * Direction=TokenSequence[TokenCount++];
      char * FLUID_Node=TokenSequence[TokenCount++];
      fluid_LADSPA_Node_t* Current_Node;
      const char * PortName=NULL;
      int CurrentPort_Index=-1;
      fluid_LADSPA_Stringmatch_t StringMatchType=fluid_LADSPA_NoMatch;
      fluid_LADSPA_Stringmatch_t CurrentPort_StringMatchType=fluid_LADSPA_NoMatch;
      CurrentPort_StringMatchCount=0;

      L(fluid_ostream_printf(out,"Wiring %s %s %s",Plugin_Port, Direction, FLUID_Node));

      /* Find the port number on the plugin, that belongs to "Plugin_Port".
       * Match the identifier specified by the user against the first characters
       * of the port name.
       *
       * If the given identifier matches several port names only partly, then the input is ambiguous, an error
       * message results.
       *
       * Example: cmt.so, limit_peak: This plugin uses the labels
       * -  Output Envelope Attack (s)
       * -  Output Envelope Decay (s)
       * -  Output
       *
       * The user input 'Output' matches the first two labels partly, the third fully. This will be accepted.
       * The user input 'Out' matches all three only partly, this results in an error message.
       */

      for (PortCount=0; PortCount<CurrentPluginDescriptor->PortCount; PortCount++){
	PortName=CurrentPluginDescriptor->PortNames[PortCount];

	StringMatchType=fluid_LADSPA_Check_SubString_Match(Plugin_Port, PortName);
	/* If a full-string match has been found earlier, reject all partial matches. */
	if (StringMatchType==fluid_LADSPA_FullMatch ||
	    (StringMatchType==fluid_LADSPA_PartialMatch && CurrentPort_StringMatchType != fluid_LADSPA_FullMatch)){

	  if(StringMatchType==fluid_LADSPA_FullMatch && CurrentPort_StringMatchType==fluid_LADSPA_FullMatch){
    fluid_ostream_printf(out, "***Error027***\n"
		     "While processing plugin %s: The port label %s appears more than once!\n"
		     "This is an error in the plugin itself. Please correct it or use another plugin.",PluginLabel, Plugin_Port);
	    fluid_LADSPA_clear(FxUnit);
	    return(PrintErrorMessage);
	  };

	  CurrentPort_Index=PortCount;
	  CurrentPort_StringMatchType=StringMatchType;
	  CurrentPort_StringMatchCount++;

	}; /* if suitable match */
      }; /* For port count */

      /* Several partial matches? Then the identifier is not unique. */
      if (CurrentPort_StringMatchCount > 1 && CurrentPort_StringMatchType == fluid_LADSPA_PartialMatch){
    fluid_ostream_printf(out, "***Error019***\n"
		 "While processing plugin %s: The identifier %s matches more than one plugin port.\n"
		 "Please use more letters for the port name. If needed, replace spaces with underscores (_).\n"
		 "This error will not occur, if you use the full name of a port.\n",PluginLabel, Plugin_Port);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };

      if (CurrentPort_Index<0){
    fluid_ostream_printf(out, "***Error017***\n"
		 "Unable to find port '%s' on plugin %s\n"
		 "Port names are:\n",Plugin_Port,PluginLabel);
	for (PortCount=0; PortCount<CurrentPluginDescriptor->PortCount; PortCount++){
	  printf("- `%s'\n",CurrentPluginDescriptor->PortNames[PortCount]);
	};

	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };
      CurrentPort_Descriptor=CurrentPluginDescriptor->PortDescriptors[CurrentPort_Index];
      assert(CurrentPort_Descriptor);

      /* Retrieve the node with the right name. */
      Current_Node=fluid_LADSPA_RetrieveNode(FxUnit,FLUID_Node);
#define PortIsAudio LADSPA_IS_PORT_AUDIO(CurrentPort_Descriptor) && !(LADSPA_IS_PORT_CONTROL(CurrentPort_Descriptor))
#define PortIsControl LADSPA_IS_PORT_CONTROL(CurrentPort_Descriptor) && !(LADSPA_IS_PORT_AUDIO(CurrentPort_Descriptor))
      if (!Current_Node){
	/* Doesn't exist? Then create it. */
	if (FxUnit->NumberNodes>=FLUID_LADSPA_MaxNodes){
    fluid_ostream_printf(out, "***Error005***\n"
		   "Too many nodes (%i)\n"
		   "Change FLUID_LADSPA_MaxNodes",FxUnit->NumberNodes);
	  fluid_LADSPA_clear(FxUnit);
	  return(PrintErrorMessage);
	};
	if (PortIsAudio){
	  Current_Node=fluid_LADSPA_CreateNode(FxUnit,FLUID_Node,fluid_LADSPA_node_is_audio);
	} else if (PortIsControl){
	  Current_Node=fluid_LADSPA_CreateNode(FxUnit,FLUID_Node,fluid_LADSPA_node_is_control);
	} else {
    fluid_ostream_printf(out, "***Error025***\n"
		   "Plugin port number %i is neither input nor output!\n"
		   "This is an error in the plugin.\n"
		   "Please check plugin sourcecode.\n",
		   CurrentPort_Index);
	  fluid_LADSPA_clear(FxUnit);
	  return(PrintErrorMessage);
	};
      };
      assert(Current_Node);

      /*
       *
       * Check flowgraph for some possible errors.
       *
       */

      if (FLUID_STRCMP(Direction,"->")==0){
	/* Data from plugin to FLUID, into node
	 *
	 * *** Rule: ****
	 * A node may not have more than one data source.*/
	if (Current_Node->InCount !=0){
    fluid_ostream_printf(out, "***Error009***\n"
		   "Plugin %s tries to feed data from output %s into node %s, which is already connected to a data source.\n",PluginLabel,Plugin_Port,FLUID_Node);
	  fluid_LADSPA_clear(FxUnit);
	  return(PrintErrorMessage);
	};
	Current_Node->InCount++;
      } else if (FLUID_STRCMP(Direction,"<-")==0){
	/* Data from FLUID to plugin, out of node
	 *
	 * This check verifies the integrity of the flow graph:
	 * *** Rule ***
	 * The execution order of the plugins is the order, in which they are programmed.
	 * The plugins must be ordered so, that the input of a plugin is already computed at the time of its execution.
	 * If the user tries to read data out of a node that has not yet an input, then something is wrong.*/
	assert(Current_Node->InCount<=1);
	if (Current_Node->InCount !=1){
    fluid_ostream_printf(out, "***Error010***\n"
		   "Plugin %s tries to read data through input %s from node %s.\n"
		   "But at this point there is no valid data at that node.\n"
		   "Please check the flowgraph and especially the execution order!\n",PluginLabel,Plugin_Port,FLUID_Node);
	  fluid_LADSPA_clear(FxUnit);
	  return(PrintErrorMessage);
	};
	Current_Node->OutCount++;
      } else {
	fluid_ostream_printf(out, "***Error024***\n"
	       "Syntax error: Illegal `arrow' `%s', expecting -> or <-\n",
	       Direction);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };

      /* In any case, there must be a valid data source for the port at this time. */
      assert(Current_Node->InCount==1);

      /* Keep track on the number of connections to each port.
       * This error occurs only, if an attempt is made to connect one port twice (i.e. ladspa_add libname pluginname port <- nodex port <- nodey) */
      if (CurrentPlugin_PortConnected[CurrentPort_Index]){
    fluid_ostream_printf(out, "***Error011***\n"
		 "Refusing to connect twice to port %s on plugin %s.\n",CurrentPluginDescriptor->PortNames[CurrentPort_Index], PluginLabel);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };

      /*
       *
       * Connect the port
       *
       */

      L(fluid_ostream_printf(out,"Connecting %i",CurrentPort_Index));
      CurrentPluginDescriptor->connect_port
	(CurrentPlugin,
	 CurrentPort_Index,
	 Current_Node->buf
	 );
      CurrentPlugin_PortConnected[CurrentPort_Index]++;

    }; /* While Tokensequence (more connections) */

    /*
     *
     * Check for left-over tokens
     *
     */

    if (TokenSequence[TokenCount]){
      char * T1="";char * T2="";char * T3="";
      if (TokenSequence[TokenCount]){T1=TokenSequence[TokenCount];};
      if (TokenSequence[TokenCount+1]){T2=TokenSequence[TokenCount+1];};
      if (TokenSequence[TokenCount+2]){T3=TokenSequence[TokenCount+2];};
    fluid_ostream_printf(out, "***Error012***\n"
	       "Leftover tokens: %s %s %s...\n",T1,T2,T3);
      fluid_LADSPA_clear(FxUnit);
      return(PrintErrorMessage);
    };

    /*
     *
     * Check, that all plugin ports are connected
     *
     */
    L(fluid_ostream_printf(out,"Checking left-over ports"));
    assert(CurrentPluginDescriptor);
    for (PortCount=0; PortCount<CurrentPluginDescriptor->PortCount; PortCount++){
      assert(CurrentPlugin_PortConnected[PortCount] <=1);
      if (CurrentPlugin_PortConnected[PortCount] !=1){
    fluid_ostream_printf(out, "***Error013***\nPlugin: %s. Port %s is unconnected!\n",PluginLabel, CurrentPluginDescriptor->PortNames[PortCount]);
	fluid_LADSPA_clear(FxUnit);
	return(PrintErrorMessage);
      };
    };

    /*
     *
     *Run activate function on plugin, where possible
     *
     */

    if (CurrentPluginDescriptor->activate !=NULL){
      CurrentPluginDescriptor->activate(CurrentPlugin);
    };

    FxUnit->NumberPlugins++;
  }; /* For CommandLineCount: once for each new command (i.e. plugin instantiation request from user */

  /*
   *
   * Further flow graph checks
   *
   */

  L(fluid_ostream_printf(out,"Checking flow graph"));

  IngoingSignalCount=0;
  OutgoingSignalCount=0;

  for (NodeCount=0; NodeCount<FxUnit->NumberNodes; NodeCount++){
    fluid_LADSPA_Node_t* Current_Node;
    Current_Node=FxUnit->Nodelist[NodeCount];
    assert(Current_Node);
    if (Current_Node->flags & fluid_LADSPA_node_is_source){
      IngoingSignalCount+=Current_Node->OutCount;
    } else if (Current_Node->flags & fluid_LADSPA_node_is_sink){
      OutgoingSignalCount+=Current_Node->InCount;
    } else {

      /* A node without any input doesn't make sense.
       * The flow graph check aborts with an error. This case cannot happen.
       */
      if (Current_Node->InCount==0 && !Current_Node->flags && fluid_LADSPA_node_is_dummy){ /* There can only be one warning at a time. */
        fluid_ostream_printf(out, "***Warning020***"
		   "No input into node %s.\n"
		   "Use '_' as first char in nodename to suppress this warning.\n"
		   "Hint: Check for typos in the node name.\n",Current_Node->Name);
	  /* A warning can also be printed as an error message (check fluid_cmd.c). The only difference between
	     the return values -1 and 0 is, that -1 prints the result. */
      };
      ReturnVal=PrintErrorMessage;
    };

    /* A node without any output doesn't make sense. */
    if (Current_Node->OutCount==0 && !Current_Node->flags && fluid_LADSPA_node_is_dummy){
      fluid_ostream_printf(out, "***Warning021***\n"
		 "No output from node %s.\n"
		 "Use '_' as first char in nodename to suppress this warning.\n"
		 "Hint: Check for typos in the node name.\n",Current_Node->Name);
      ReturnVal=PrintErrorMessage;
    };
    /* A free-flying node simply cannot happen. */
    assert(Current_Node->OutCount+Current_Node->InCount);
  }; /* Foreach node */

  /* Issue a warning, if no signal goes into the Fx section. */
  if (IngoingSignalCount==0){
    fluid_ostream_printf(out, "***Warning022***\n"
	       "You have not connected anything to the synthesizer section (in1_L, in1_R).\n");
    ReturnVal=PrintErrorMessage;
  };

  /* Issue a warning, if no signal leaves the Fx section. */
  if (OutgoingSignalCount==0){
    fluid_ostream_printf(out, "***Warning023***\n"
	       "You have not connected anything to the output (out1_L, out1_R).\n");
  };

  /* Finally turn on the Fx unit. */
  FxUnit->Bypass=fluid_LADSPA_Active;
  L(fluid_ostream_printf(out,"LADSPA Init OK"));
  return(ReturnVal);
};

void
fluid_LADSPA_run(fluid_LADSPA_FxUnit_t* FxUnit, fluid_real_t* left_buf[], fluid_real_t* right_buf[], fluid_real_t* fx_left_buf[], fluid_real_t* fx_right_buf[]){
  int i;
  int ii;


  int nr_audio_channels;
  int nr_fx_sends;
  int nr_groups;
  int byte_size = FLUID_BUFSIZE * sizeof(fluid_real_t);
  char str[99];
  fluid_LADSPA_Node_t* n;
  int temp;

  /* Retrieve the number of synth / audio out / Fx send nodes */
  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.audio-groups", &temp));
  nr_groups=(int) temp;

  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.audio-channels", &temp));
  nr_audio_channels=temp;

  assert(fluid_settings_getint(FxUnit->synth->settings, "synth.effects-channels", &temp));
  nr_fx_sends=temp;

  /* Fixme: Retrieving nodes via names is inefficient
   * (but not that bad, because the interesting nodes are always at the start of the list).
   */

  /* Input and output are processed via the same buffers. Therefore the effect is bypassed by just skipping everything else. */
  if (FxUnit->Bypass==fluid_LADSPA_Bypassed){
    return;
  };

  if (FxUnit->Bypass==fluid_LADSPA_BypassRequest){
    FxUnit->Bypass=fluid_LADSPA_Bypassed;
    pthread_mutex_lock(&FxUnit->mutex);
    pthread_cond_broadcast(&FxUnit->cond);
    pthread_mutex_unlock(&FxUnit->mutex);
    L(printf("LADSPA_Run: Command line asked for bypass of Fx unit. Acknowledged."));
    return;
  };

  assert(FxUnit);

  /* Clear the output buffers, if they are not connected anywhere */
  for (ii=0; ii < nr_audio_channels; ii++){

      sprintf(str, "out%i_L",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      if (n->InCount == 0){
/*	  printf("Output node %s is not connected -> clear buffer\n", str); */
	  FLUID_MEMSET(n->buf, 0, byte_size);
      };

      sprintf(str, "out%i_R",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      if (n->InCount == 0){
/*	  printf("Output node %s is not connected -> clear buffer\n", str); */
	  FLUID_MEMSET(n->buf, 0, byte_size);
      };
  };

  /* Prepare the incoming data:
   * Convert fluid_real_t data type to LADSPA_Data type */
  for (ii=0; ii < nr_groups; ii++){
      fluid_real_t* src_buf=left_buf[ii];
      sprintf(str, "in%i_L",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);

      assert(FLUID_BUFSIZE % 2 == 0);

      /* Add a very small high frequency signal. This avoids denormal number problems. */
      for (i=0; i<FLUID_BUFSIZE;){
	  n->buf[i]=(LADSPA_Data)(src_buf[i]+1.e-15);
	  i++;
	  n->buf[i]=(LADSPA_Data)(src_buf[i]);
	  i++;
      };

      src_buf=right_buf[ii];
      sprintf(str, "in%i_R",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);

      /* Add a very small high frequency signal. This avoids denormal number problems. */
      for (i=0; i<FLUID_BUFSIZE;){
	  n->buf[i]=(LADSPA_Data)(src_buf[i]+1.e-15);
	  i++;
	  n->buf[i]=(LADSPA_Data)(src_buf[i]);
	  i++;
      };
  };

  /* Effect send paths */
  for (ii=0; ii < nr_fx_sends; ii++){
      sprintf(str, "send%i_L",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      for (i=0; i<FLUID_BUFSIZE; i++){
	  n->buf[i]=(LADSPA_Data)(fx_left_buf[ii][i]);
      };

      sprintf(str, "send%i_R",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      for (i=0; i<FLUID_BUFSIZE; i++){
	  n->buf[i]=(LADSPA_Data)(fx_right_buf[ii][i]);
      };
  };

  /* Run each plugin on a block of data.
   * The execution order has been checked during setup.*/
  for (i=0; i<FxUnit->NumberPlugins; i++){
    FxUnit->PluginDescriptorTable[i]->run(FxUnit->PluginInstanceTable[i],FLUID_BUFSIZE);
  };

  /* Copy the data from the output nodes back to the synth. */
  for (ii=0; ii < nr_audio_channels; ii++){
      fluid_real_t* dest_buf=left_buf[ii];
      sprintf(str, "out%i_L",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      for (i=0; i<FLUID_BUFSIZE; i++){
	  dest_buf[i]=(fluid_real_t)n->buf[i];
      };

      dest_buf=right_buf[ii];
      sprintf(str, "out%i_R",(ii+1));
      n=fluid_LADSPA_RetrieveNode(FxUnit, str); assert(n);
      for (i=0; i<FLUID_BUFSIZE; i++){
	  dest_buf[i]=(fluid_real_t)n->buf[i];
      };
  };
};

fluid_LADSPA_Node_t*
fluid_LADSPA_RetrieveNode(fluid_LADSPA_FxUnit_t* FxUnit, char * Name){
  int i=0;
  assert(FxUnit);assert(Name);
  for (i=0; i<FxUnit->NumberNodes; i++){
    assert(FxUnit->Nodelist[i]);
    if (FLUID_STRCMP(FxUnit->Nodelist[i]->Name,Name)==0){
      return FxUnit->Nodelist[i];
    };
  };
  return NULL;
};


/* Purpose:
 * Creates a new node from the node name given by the user.
 */
fluid_LADSPA_Node_t*
fluid_LADSPA_CreateNode(fluid_LADSPA_FxUnit_t* FxUnit, char * Name, int flags){
  int Dummy=0;
  fluid_LADSPA_Node_t* NewNode;
  assert(FxUnit);
  assert(Name);
//  printf("Flags is %i\n",flags);
  L(printf("Create node: %s",Name));
  if (FxUnit->NumberNodes>=FLUID_LADSPA_MaxNodes){
    printf( "***Error014***\n"
	    "Too many nodes (%i)\n"
	    "Change FLUID_LADSPA_MaxNodes.\n",FxUnit->NumberNodes);
    fluid_LADSPA_clear(FxUnit);
    return NULL;
  };

  /* Don't allow node names, which start with -, 0..9 */
  if (Name[0] == '-' || (Name[0]>='0' && Name[0]<='9')){
    printf( "***Error026***\n"
	    "The node name %s starts with a digit / minus sign!\n"
	    "Please use a letter to start a node name.\n"
	    "A constant node is created by using `#' as first character,\n"
	    "for example #-2.5.\n",
	    Name);
    fluid_LADSPA_clear(FxUnit);
    return NULL;
  };

  /* A nodename starting with "_" is a possible dummy node, which may (but need not) act as a data sink or source (dummy node). */
  if (Name[0] == ' '){ /* ??? Should be '_' ??? */
    Dummy=1;
  };
  NewNode=FLUID_NEW(fluid_LADSPA_Node_t);assert(NewNode);
  if (flags && fluid_LADSPA_node_is_audio){
    /* Audio node contains buffer. */
    NewNode->buf=FLUID_ARRAY(LADSPA_Data, (FLUID_BUFSIZE));assert(NewNode->buf);
    /* It is permitted to use a dummy node without input. Therefore clear all node buffers at startup. */
    FLUID_MEMSET(NewNode->buf, 0, (FLUID_BUFSIZE*sizeof(LADSPA_Data)));
  } else if (flags & fluid_LADSPA_node_is_control){
    /* Control node contains single value. */
    NewNode->buf=FLUID_ARRAY(LADSPA_Data, 1);assert(NewNode->buf);
  } else {
    assert(0);
  };
  NewNode->Name=FLUID_STRDUP(Name);assert(NewNode->Name);
  if (Dummy){
      flags |= fluid_LADSPA_node_is_dummy;
  };
  NewNode->InCount=0;
  NewNode->OutCount=0;
  NewNode->flags=flags;

  /* A nodename starting with "#" means that the node holds a constant value. */
  if (NewNode->Name[0] == '#'){
    assert(flags & fluid_LADSPA_node_is_control);
    /* Skip the first character => +1 */
    NewNode->buf[0]=(LADSPA_Data)atof(NewNode->Name+1);
    NewNode->InCount++;
  };
  if (flags & fluid_LADSPA_node_is_source){
    NewNode->InCount++;
//    printf("****************************** Source!\n");
  } else if (flags & fluid_LADSPA_node_is_sink){
    NewNode->OutCount++;
  };
  FxUnit->Nodelist[FxUnit->NumberNodes++]=NewNode;

  L(printf("Node %s created.",Name));
  return NewNode;
};

void fluid_LADSPA_clear(fluid_LADSPA_FxUnit_t* FxUnit){
  int i;
  int ii;
  L(printf("ladspa_clear"));
  assert(FxUnit);

  if (FxUnit->Bypass==fluid_LADSPA_Active){
    L(printf("clear: Requesting bypass from synthesis thread"));
    /* Bypass the Fx unit before anything else.
     * Reason: Not a good idea to release plugins, while another thread runs them.
     */
    FxUnit->Bypass=fluid_LADSPA_BypassRequest;
    pthread_mutex_lock(&FxUnit->mutex);
    pthread_cond_wait(&FxUnit->cond,&FxUnit->mutex);
    pthread_mutex_unlock(&FxUnit->mutex);
    L(printf("clear: Synthesis thread has switched to bypass."));
  } else {
    L(printf("clear: Fx unit was already bypassed. No action needed."));
  };

  L(printf("Clear all user control node declarations"));
  for (i=0; i<FxUnit->NumberUserControlNodes; i++){
    FLUID_FREE(FxUnit->UserControlNodeNames[i]);
  };
  FxUnit->NumberUserControlNodes=0;

  L(printf("Clear all plugin instances"));
  for (i=0; i<FxUnit->NumberPlugins; i++){
    assert(FxUnit->PluginDescriptorTable[i]);
    assert(FxUnit->PluginInstanceTable[i]);

    /* Run deactivate function on plugin, if possible */
    if (FxUnit->PluginDescriptorTable[i]->deactivate){
      FxUnit->PluginDescriptorTable[i]->deactivate(FxUnit->PluginInstanceTable[i]);
    };
    FxUnit->PluginDescriptorTable[i]->cleanup(FxUnit->PluginInstanceTable[i]);
  };
  FxUnit->NumberPlugins=0;

  L(printf("Clear all nodes")); /* Only after removing plugins! */
  for (i=0; i<FxUnit->NumberNodes; i++){
    FLUID_FREE(FxUnit->Nodelist[i]->buf);
    FLUID_FREE(FxUnit->Nodelist[i]);
  };
  FxUnit->NumberNodes=0;


  L(printf("Clear all plugin libraries"));
  for (i=0; i<FxUnit->NumberLibs; i++){

    assert(FxUnit->ppvPluginLibs[i]);
    dlclose(FxUnit->ppvPluginLibs[i]);

    assert(FxUnit->ppvPluginLibNames[i]);
    FLUID_FREE(FxUnit->ppvPluginLibNames[i]);
  };
  FxUnit->NumberLibs=0;

  L(printf("Clear all command lines"));
  for (i=0; i<FxUnit->NumberCommands;i++){
    ii=0;
    assert(FxUnit->LADSPA_Command_Sequence[i]);
    while (FxUnit->LADSPA_Command_Sequence[i][ii]){
      FLUID_FREE(FxUnit->LADSPA_Command_Sequence[i][ii]);
      ii++;
    };
    FLUID_FREE(FxUnit->LADSPA_Command_Sequence[i]);
  };
  FxUnit->NumberCommands=0;
};

int fluid_LADSPA_handle_add(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out){
  int i;
  //char * Token;
  char ** CommandLine;
  fluid_LADSPA_FxUnit_t* FxUnit;
  assert(synth);
  FxUnit=synth->LADSPA_FxUnit; assert(FxUnit);
  if (ac>=FLUID_LADSPA_MaxTokens){
    /* Can't be tested. fluidsynth limits the number of tokens. */
    printf("***Error001***\n"
	     "Too many ports.\nChange FLUID_LADSPA_MaxTokens!");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };
  if (ac<2){
    printf("***Error002***\n"
	     "ladspa_add needs at least two arguments - libname and plugin name!");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };

  if (FxUnit->NumberCommands>=FLUID_LADSPA_MaxPlugins){
    printf("***Error032***\n"
	     "Too many plugins.\nChange FLUID_LADSPA_MaxPlugins!");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };

  /* CommandLine (token sequence) is terminated with NULL.
   * Add two more NULLs, so that a chunk of three tokens can be checked later without risk.*/

  CommandLine=FLUID_ARRAY(char*, (ac+3));assert(CommandLine);
  for (i=0; i<ac; i++){
    CommandLine[i]=FLUID_STRDUP(av[i]);assert(CommandLine[i]);
  };
  CommandLine[ac]=NULL;
  CommandLine[ac+1]=NULL;
  CommandLine[ac+2]=NULL;

  FxUnit->LADSPA_Command_Sequence[FxUnit->NumberCommands]=CommandLine;
  FxUnit->NumberCommands++;
  return(FLUID_OK);
};

int fluid_LADSPA_handle_declnode(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out){
  //int i;
  //char * Token;
  //char ** CommandLine;
  char * NodeName;
  fluid_real_t NodeValue;
  fluid_LADSPA_FxUnit_t* FxUnit;
  assert(synth);
  FxUnit=synth->LADSPA_FxUnit; assert(FxUnit);

  if (ac<2){
    printf("***Error028***\n"
	   "ladspa_declnode needs two arguments - node name and value!\n");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };

  if (FxUnit->NumberUserControlNodes>=FLUID_LADSPA_MaxNodes){
    printf("***Error033***\n"
	     "Too many user-control nodes.\nChange FLUID_LADSPA_MaxNodes!");
    fluid_LADSPA_clear(FxUnit);
    return(PrintErrorMessage);
  };

  NodeName=FLUID_STRDUP(av[0]); assert(NodeName);
  NodeValue=atof(av[1]);
  FxUnit->UserControlNodeNames[FxUnit->NumberUserControlNodes]=NodeName;
  FxUnit->UserControlNodeValues[FxUnit->NumberUserControlNodes]=NodeValue;
  FxUnit->NumberUserControlNodes++;
  return(FLUID_OK);
};
int fluid_LADSPA_handle_setnode(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out){
  //int i;
  //char * Token;
  char * NodeName;
  fluid_real_t NodeValue;
  fluid_LADSPA_FxUnit_t* FxUnit;
  fluid_LADSPA_Node_t* CurrentNode;
  assert(synth);
  FxUnit=synth->LADSPA_FxUnit; assert(FxUnit);

  if (ac!=2){
    printf("***Error029***\n"
	   "ladspa_setnode needs two arguments - node name and value!\n");
    /* Do not clear the Fx unit (no fluid_LADSPA_clear). */
    return(PrintErrorMessage);
  };

  NodeName=av[0]; assert(NodeName);
  NodeValue=atof(av[1]);

  CurrentNode=fluid_LADSPA_RetrieveNode(FxUnit,NodeName);
  if (!CurrentNode){
    printf("***Error030***\n"
	   "The node %s was not found. Please use the full name of a node, that was\n"
	   "previously declared with ladspa_declnode.\n",NodeName);
    /* Do not clear the Fx unit (no fluid_LADSPA_clear). */
    return(PrintErrorMessage);
  };
  if (!(CurrentNode->flags & fluid_LADSPA_node_is_user_ctrl)){
    printf("***Error031***\n"
	   "The node %s is an ordinary control node.\n"
	   "Only user control nodes can be modified with ladspa_setnode.\n",NodeName);
    /* Do not clear the Fx unit (no fluid_LADSPA_clear). */
    return(PrintErrorMessage);
  };
  L(printf("ladspa_setnode: Assigning value %f",NodeValue));
  CurrentNode->buf[0]=NodeValue;
  return(FLUID_OK);
};

int fluid_LADSPA_handle_clear(fluid_synth_t* synth, int ac, char** av, fluid_ostream_t out){
  fluid_LADSPA_FxUnit_t* FxUnit;
  assert(synth);
  FxUnit=synth->LADSPA_FxUnit; assert(FxUnit);
  fluid_LADSPA_clear(FxUnit);
  return(FLUID_OK);
};

void fluid_LADSPA_shutdown(fluid_LADSPA_FxUnit_t* FxUnit){
  /* The synthesis thread is not running anymore.
   * Set the bypass switch, so that fluid_LADSPA_clear can proceed.*/
  FxUnit->Bypass=fluid_LADSPA_Bypassed;
  fluid_LADSPA_clear(FxUnit);
  pthread_cond_destroy(&FxUnit->cond); /* pro forma */
};
#endif /*LADSPA*/
