/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "dosbox.h"
#include "cross.h"
#include "setup.h"
#include "control.h"
#include "support.h"
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <limits>
#include <limits.h>

/* functions to call when DOSBox-X is exiting. */
std::list<Function_wrapper> exitfunctions;

/* VM events */
std::list<Function_wrapper> vm_event_functions[VM_EVENT_MAX];

using namespace std;
static std::string current_config_dir; // Set by parseconfigfile so Prop_path can use it to construct the realpath
void Value::destroy() throw(){
	if (type == V_STRING) delete _string;
	_string = NULL;
}

Value& Value::copy(Value const& in) throw(WrongType) {
	if (this != &in) { //Selfassigment!
		if(type != V_NONE && type != in.type) throw WrongType();
		destroy();
		plaincopy(in);
	}
	return *this;
}

void Value::plaincopy(Value const& in) throw(){
	type = in.type;
	_int = in._int;
	_double = in._double;
	_bool = in._bool;
	_hex = in._hex;
	if(type == V_STRING) _string = new string(*in._string);
}

Value::operator bool () const throw(WrongType) {
	if(type != V_BOOL) throw WrongType();
	return _bool;
}

Value::operator Hex () const throw(WrongType) {
	if(type != V_HEX) throw WrongType();
	return _hex;
}

Value::operator int () const throw(WrongType) {
	if(type != V_INT) throw WrongType();
	return _int;
}

Value::operator double () const throw(WrongType) {
	if(type != V_DOUBLE) throw WrongType();
	return _double;
}

Value::operator char const* () const throw(WrongType) {
	if(type != V_STRING) throw WrongType();
	return _string->c_str();
}

bool Value::operator==(Value const& other) {
	if(this == &other) return true;
	if(type != other.type) return false;
	switch(type){
		case V_BOOL: 
			if(_bool == other._bool) return true;
			break;
		case V_INT:
			if(_int == other._int) return true;
			break;
		case V_HEX:
			if(_hex == other._hex) return true;
			break;
		case V_DOUBLE:
			if(_double == other._double) return true;
			break;
		case V_STRING:
			if((*_string) == (*other._string)) return true;
			break;
		default:
			E_Exit("comparing stuff that doesn't make sense");
			break;
	}
	return false;
}
bool Value::SetValue(string const& in,Etype _type) throw(WrongType) {
	/* Throw exception if the current type isn't the wanted type 
	 * Unless the wanted type is current.
	 */
	if(_type == V_CURRENT && type == V_NONE) throw WrongType();
	if(_type != V_CURRENT) { 
		if(type != V_NONE && type != _type) throw WrongType();
		type = _type;
	}
	bool retval = true;
	switch(type){
		case V_HEX:
			retval = set_hex(in);
			break;
		case V_INT:
			retval = set_int(in);
			break;
		case V_BOOL:
			retval = set_bool(in);
			break;
		case V_STRING:
			set_string(in);
			break;
		case V_DOUBLE:
			retval = set_double(in);
			break;

		case V_NONE:
		case V_CURRENT:
		default:
			/* Shouldn't happen!/Unhandled */
			throw WrongType();
			break;
	}
	return retval;
}

bool Value::set_hex(std::string const& in) {
	istringstream input(in);
	input.flags(ios::hex);
	Bits result = INT_MIN;
	input >> result;
	if(result == INT_MIN) return false;
	_hex = result;
	return true;
}

bool Value::set_int(string const &in) {
	istringstream input(in);
	Bits result = INT_MIN;
	input >> result;
	if(result == INT_MIN) return false;
	_int = result;
	return true;
}
bool Value::set_double(string const &in) {
	istringstream input(in);
	double result = std::numeric_limits<double>::infinity();
	input >> result;
	if(result == std::numeric_limits<double>::infinity()) return false;
	_double = result;
	return true;
}

bool Value::set_bool(string const &in) {
	istringstream input(in);
	string result;
	input >> result;
	lowcase(result);
	_bool = true; // TODO
	if(!result.size()) return false;
	
	if(result=="0" || result=="disabled" || result=="false" || result=="off") {
		_bool = false;
	} else if(result=="1" || result=="enabled" || result=="true" || result=="on") {
		_bool = true;
	} else return false;
	
	return true;
}

void Value::set_string(string const & in) {
	if(!_string) _string = new string();
	_string->assign(in);
}

string Value::ToString() const {
	ostringstream oss;
	switch(type) {
		case V_HEX:
			oss.flags(ios::hex);
			oss << _hex;
			break;
		case V_INT:
			oss << _int;
			break;
		case V_BOOL:
			oss << boolalpha << _bool;
			break;
		case V_STRING:
			oss << *_string;
			break;
		case V_DOUBLE:
			oss.precision(2);
			oss << fixed << _double;
			break;
		case V_NONE:
		case V_CURRENT:
		default:
			E_Exit("ToString messed up ?");
			break;
	}
	return oss.str();
}

bool Property::CheckValue(Value const& in, bool warn){
	if(suggested_values.empty()) return true;
	for(iter it = suggested_values.begin();it != suggested_values.end();it++) {
		if ( (*it) == in) { //Match!
			return true;
		}
	}
	if(warn) LOG_MSG("\"%s\" is not a valid value for variable: %s.\nIt might now be reset to the default value: %s",in.ToString().c_str(),propname.c_str(),default_value.ToString().c_str());
	return false;
}

/* There are too many reasons I can think of to have similar property names per section
 * without tying them together by name. Sticking them in MSG_Add as CONFIG_ + propname
 * for help strings is just plain dumb. But... in the event that is still useful, I at
 * least left the code conditionally enabled if any part of the code still wants to do
 * that. --J.C */
void Property::Set_help(string const& in) {
	if (use_global_config_str) {
		string result = string("CONFIG_") + propname;
		upcase(result);
		MSG_Add(result.c_str(),in.c_str());
	}
	else {
		help_string = in;
	}
}

char const* Property::Get_help() {
	if (use_global_config_str) {
		string result = string("CONFIG_") + propname;
		upcase(result);
		return MSG_Get(result.c_str());
	}

	return help_string.c_str();
}


bool Prop_int::CheckValue(Value const& in, bool warn) {
	if(suggested_values.empty() && Property::CheckValue(in,warn)) return true;
	//No >= and <= in Value type and == is ambigious
	int mi = min;
	int ma = max;
	int va = static_cast<int>(Value(in));
	if(mi == -1 && ma == -1) return true;
	if (va >= mi && va <= ma) return true;
	if(warn) LOG_MSG("%s lies outside the range %s-%s for variable: %s.\nIt might now be reset to the default value: %s",in.ToString().c_str(),min.ToString().c_str(),max.ToString().c_str(),propname.c_str(),default_value.ToString().c_str());
	return false;
}

bool Prop_double::SetValue(std::string const& input){
	Value val;
	if(!val.SetValue(input,Value::V_DOUBLE)) return false;
	return SetVal(val,false,/*warn*/true);
}

bool Prop_int::SetValue(std::string const& input){;
	Value val;
	if(!val.SetValue(input,Value::V_INT)) return false;
	return SetVal(val,false,/*warn*/true);
}

bool Prop_string::SetValue(std::string const& input){
	//Special version for lowcase stuff
	std::string temp(input);
	//suggested values always case insensitive. 
	//If there are none then it can be paths and such which are case sensitive
	if(!suggested_values.empty()) lowcase(temp);
	Value val(temp,Value::V_STRING);
	return SetVal(val,false,true);
}
bool Prop_string::CheckValue(Value const& in, bool warn){
	if(suggested_values.empty()) return true;
	for(iter it = suggested_values.begin();it != suggested_values.end();it++) {
		if ( (*it) == in) { //Match!
			return true;
		}
		if((*it).ToString() == "%u") {
			Bit32u value;
			if(sscanf(in.ToString().c_str(),"%u",&value) == 1) {
				return true;
			}
		}
	}
	if(warn) LOG_MSG("\"%s\" is not a valid value for variable: %s.\nIt might now be reset to the default value: %s",in.ToString().c_str(),propname.c_str(),default_value.ToString().c_str());
	return false;
}

bool Prop_path::SetValue(std::string const& input){
	//Special version to merge realpath with it

	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true);

	if(input.empty()) {
		realpath = "";
		return false;
	}
	std::string workcopy(input);
	Cross::ResolveHomedir(workcopy); //Parse ~ and friends
	//Prepend config directory in it exists. Check for absolute paths later
	if( current_config_dir.empty()) realpath = workcopy;
	else realpath = current_config_dir + CROSS_FILESPLIT + workcopy;
	//Absolute paths
	if (Cross::IsPathAbsolute(workcopy)) realpath = workcopy;
	return retval;
}
	
bool Prop_bool::SetValue(std::string const& input){
	Value val;
	if(!val.SetValue(input,Value::V_BOOL)) return false;
	return SetVal(val,false,/*warn*/true);
}

bool Prop_hex::SetValue(std::string const& input){
	Value val;
	if(!val.SetValue(input,Value::V_HEX)) return false;
	return SetVal(val,false,/*warn*/true);
}

void Prop_multival::make_default_value(){
	Bitu i = 1;
	Property *p = section->Get_prop(0);
	if(!p) return;

	std::string result = p->Get_Default_Value().ToString();
	while( (p = section->Get_prop(i++)) ) {
		std::string props = p->Get_Default_Value().ToString();
		if(props == "") continue;
		result += seperator; result += props;
	}
	Value val(result,Value::V_STRING);
	SetVal(val,false,true,/*init*/true);
}

   

//TODO checkvalue stuff
bool Prop_multival_remain::SetValue(std::string const& input,bool init) {
	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true,init);

	std::string local(input);
	int i = 0,number_of_properties = 0;
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if(!p) return false;
	
	while( (section->Get_prop(number_of_properties)) )
		number_of_properties++;
	
	string::size_type loc = string::npos;
	while( (p = section->Get_prop(i++)) ) {
		//trim leading seperators
		loc = local.find_first_not_of(seperator);
		if(loc != string::npos) local.erase(0,loc);
		loc = local.find_first_of(seperator);
		string in = "";//default value
		/* when i == number_of_properties add the total line. (makes more then 
		 * one string argument possible for parameters of cpu) */
		if(loc != string::npos && i < number_of_properties) { //seperator found 
			in = local.substr(0,loc);
			local.erase(0,loc+1);
		} else if(local.size()) { //last argument or last property
			in = local;
			local = "";
		}
		//Test Value. If it fails set default
		Value valtest (in,p->Get_type());
		if(!p->CheckValue(valtest,true)) {
			make_default_value();
			return false;
		}
		p->SetValue(in);
	}
	return retval;
}

//TODO checkvalue stuff
bool Prop_multival::SetValue(std::string const& input,bool init) {
	Value val(input,Value::V_STRING);
	bool retval = SetVal(val,false,true,init);

	std::string local(input);
	int i = 0;
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if(!p) return false;
	string::size_type loc = string::npos;
	while( (p = section->Get_prop(i++)) ) {
		//trim leading seperators
		loc = local.find_first_not_of(seperator);
		if(loc != string::npos) local.erase(0,loc);
		loc = local.find_first_of(seperator);
		string in = "";//default value
		if(loc != string::npos) { //seperator found
			in = local.substr(0,loc);
			local.erase(0,loc+1);
		} else if(local.size()) { //last argument
			in = local;
			local = "";
		}
		//Test Value. If it fails set default
		Value valtest (in,p->Get_type());
		if(!p->CheckValue(valtest,true)) {
			make_default_value();
			return false;
		}
		p->SetValue(in);

	}
	return retval;
}

const std::vector<Value>& Property::GetValues() const {
	return suggested_values;
}
const std::vector<Value>& Prop_multival::GetValues() const 
{
	Property *p = section->Get_prop(0);
	//No properties in this section. do nothing
	if(!p) return suggested_values;
	int i =0;
	while( (p = section->Get_prop(i++)) ) {
		std::vector<Value> v = p->GetValues();
		if(!v.empty()) return p->GetValues();
	}
	return suggested_values;
}

void Property::Set_values(const char * const *in) {
	Value::Etype type = default_value.type;
	int i = 0;
	while (in[i]) {
		Value val(in[i],type);
		suggested_values.push_back(val);
		i++;
	}
}

Prop_double* Section_prop::Add_double(string const& _propname, Property::Changeable::Value when, double _value) {
	Prop_double* test=new Prop_double(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_int* Section_prop::Add_int(string const& _propname, Property::Changeable::Value when, int _value) {
	Prop_int* test=new Prop_int(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_string* Section_prop::Add_string(string const& _propname, Property::Changeable::Value when, char const * const _value) {
	Prop_string* test=new Prop_string(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_path* Section_prop::Add_path(string const& _propname, Property::Changeable::Value when, char const * const _value) {
	Prop_path* test=new Prop_path(_propname,when,_value);
	properties.push_back(test);
	return test;
}

Prop_bool* Section_prop::Add_bool(string const& _propname, Property::Changeable::Value when, bool _value) {
	Prop_bool* test=new Prop_bool(_propname,when,_value);
	properties.push_back(test);
	return test;
}
Prop_hex* Section_prop::Add_hex(string const& _propname, Property::Changeable::Value when, Hex _value) {
	Prop_hex* test=new Prop_hex(_propname,when,_value);
	properties.push_back(test);
	return test;
}
Prop_multival* Section_prop::Add_multi(std::string const& _propname, Property::Changeable::Value when,std::string const& sep) {
	Prop_multival* test = new Prop_multival(_propname,when,sep);
	properties.push_back(test);
	return test;
}
Prop_multival_remain* Section_prop::Add_multiremain(std::string const& _propname, Property::Changeable::Value when,std::string const& sep) {
	Prop_multival_remain* test = new Prop_multival_remain(_propname,when,sep);
	properties.push_back(test);
	return test;
}

int Section_prop::Get_int(string const&_propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

bool Section_prop::Get_bool(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return false;
}
double Section_prop::Get_double(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return 0.0;
}

Prop_path* Section_prop::Get_path(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			Prop_path* val = dynamic_cast<Prop_path*>((*tel));
			if(val) return val; else return NULL;
		}
	}
	return NULL;
}

Prop_multival* Section_prop::Get_multival(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			Prop_multival* val = dynamic_cast<Prop_multival*>((*tel));
			if(val) return val; else return NULL;
		}
	}
	return NULL;
}

Prop_multival_remain* Section_prop::Get_multivalremain(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			Prop_multival_remain* val = dynamic_cast<Prop_multival_remain*>((*tel));
			if(val) return val; else return NULL;
		}
	}
	return NULL;
}
Property* Section_prop::Get_prop(int index){
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if(!index--) return (*tel);
	}
	return NULL;
}

const char* Section_prop::Get_string(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return "";
}
Hex Section_prop::Get_hex(string const& _propname) const {
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if((*tel)->propname==_propname){
			return ((*tel)->GetValue());
		}
	}
	return 0;
}

void trim(string& in) {
	string::size_type loc = in.find_first_not_of(" \r\t\f\n");
	if(loc != string::npos) in.erase(0,loc);
	loc = in.find_last_not_of(" \r\t\f\n");
	if(loc != string::npos) in.erase(loc+1);
}

//TODO double c_str
bool Section_prop::HandleInputline(string const& gegevens){
	string str1 = gegevens;
	string::size_type loc = str1.find('=');
	if(loc == string::npos) return false;
	string name = str1.substr(0,loc);
	string val = str1.substr(loc + 1);
	/* trim the results incase there were spaces somewhere */
	trim(name);trim(val);
	for(it tel=properties.begin();tel!=properties.end();tel++){
		if(!strcasecmp((*tel)->propname.c_str(),name.c_str())){
			return (*tel)->SetValue(val);
		}
	}
	return false;
}

void Section_prop::PrintData(FILE* outfile,bool everything) {
	/* Now print out the individual section entries */
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if (!everything && !(*tel)->modified()) continue;

		fprintf(outfile,"%s=%s\n",(*tel)->propname.c_str(),(*tel)->GetValue().ToString().c_str());
	}
}

//TODO geen noodzaak voor 2 keer c_str
string Section_prop::GetPropValue(string const& _property) const{
	for(const_it tel=properties.begin();tel!=properties.end();tel++){
		if(!strcasecmp((*tel)->propname.c_str(),_property.c_str())){
			return (*tel)->GetValue().ToString();
		}
	}
	return NO_SUCH_PROPERTY;
}

/* NTS: For whatever reason, this string concatenation is reported by Valgrind as a memory
 *      leak because std::string's destructor is never given a chance to clear it, or something...
 *      I can't quite pinpoint why. Not enough information is provided so far in stack traces.
 *      It SHOULD be freed, because Section_line is derived from Section, the constructors are
 *      virtual, and therefore std::string should get a chance to free it's memory. */
bool Section_line::HandleInputline(string const& line){ 
	data+=line;
	data+="\n";
	return true;
}

void Section_line::PrintData(FILE* outfile,bool everything) {
	fprintf(outfile,"%s",data.c_str());
}

string Section_line::GetPropValue(string const& /* _property*/) const {
	return NO_SUCH_PROPERTY;
}

bool Config::PrintConfig(char const * const configfilename,bool everything) const {
	char temp[50];char helpline[256];
	FILE* outfile=fopen(configfilename,"w+t");
	if(outfile==NULL) return false;

	/* Print start of configfile and add an return to improve readibility. */
	fprintf(outfile,MSG_Get("CONFIGFILE_INTRO"),VERSION);
	fprintf(outfile,"\n");
	for (const_it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){
		/* Print out the Section header */
		Section_prop *sec = dynamic_cast<Section_prop *>(*tel);
		strcpy(temp,(*tel)->GetName());
		lowcase(temp);

		if (sec) {
			int mods=0;
			Property *p;
			size_t i = 0, maxwidth = 0;
			while ((p = sec->Get_prop(i++))) {
				if (!everything && !p->modified()) continue;

				size_t w = strlen(p->propname.c_str());
				if (w > maxwidth) maxwidth = w;
				mods++;
			}

			if (!everything && mods == 0) {
				/* nothing to print */
				continue;
			}

			fprintf(outfile,"[%s]\n",temp);

			i=0;
			char prefix[80];
			snprintf(prefix,80, "\n# %*s  ", (int)maxwidth, "");
			while ((p = sec->Get_prop(i++))) {
				if (!everything && !p->modified()) continue;

				std::string help = p->Get_help();
				std::string::size_type pos = std::string::npos;
				while ((pos = help.find("\n", pos+1)) != std::string::npos) {
					help.replace(pos, 1, prefix);
				}

				std::vector<Value> values = p->GetValues();

				if (help != "" || !values.empty()) {
					fprintf(outfile, "# %*s: %s", (int)maxwidth, p->propname.c_str(), help.c_str());

					if (!values.empty()) {
						fprintf(outfile, "%s%s:", prefix, MSG_Get("CONFIG_SUGGESTED_VALUES"));
						std::vector<Value>::iterator it = values.begin();
						while (it != values.end()) {
							if((*it).ToString() != "%u") { //Hack hack hack. else we need to modify GetValues, but that one is const...
								if (it != values.begin()) fputs(",", outfile);
								fprintf(outfile, " %s", (*it).ToString().c_str());
							}
							++it;
						}
						fprintf(outfile,".");
					}
					fprintf(outfile, "\n");
				}
			}
		} else {
			fprintf(outfile,"[%s]\n",temp);

			upcase(temp);
			strcat(temp,"_CONFIGFILE_HELP");
			const char * helpstr=MSG_Get(temp);
			char * helpwrite=helpline;
			while (*helpstr) {
				*helpwrite++=*helpstr;
				if (*helpstr == '\n') {
					*helpwrite=0;
					fprintf(outfile,"# %s",helpline);
					helpwrite=helpline;
				}
				helpstr++;
			}
		}
	   
		(*tel)->PrintData(outfile,everything);
		fprintf(outfile,"\n");		/* Always an empty line between sections */
	}
	fclose(outfile);
	return true;
}
   

Section_prop* Config::AddSection_prop(char const * const _name,void (*_initfunction)(Section*),bool canchange){
	Section_prop* blah = new Section_prop(_name);
	sectionlist.push_back(blah);
	return blah;
}

Section_prop::~Section_prop() {
	/* Delete properties themself (properties stores the pointer of a prop */
	for(it prop = properties.begin(); prop != properties.end(); prop++) delete (*prop);
	properties.clear();
}


Section_line* Config::AddSection_line(char const * const _name,void (*_initfunction)(Section*)){
	Section_line* blah = new Section_line(_name);
	sectionlist.push_back(blah);
	return blah;
}

void Null_Init(Section *sec);

void AddExitFunction(SectionFunction func,const char *name,bool canchange) {
	/* NTS: Add functions so that iterating front to back executes them in First In Last Out order. */
	exitfunctions.push_front(Function_wrapper(func,canchange,name));
}

void AddVMEventFunction(enum vm_event event,SectionFunction func,const char *name,bool canchange) {
	assert(event < VM_EVENT_MAX);

	/* NTS: First In First Out order */
	vm_event_functions[event].push_back(Function_wrapper(func,canchange,name));
}

const char *VM_EVENT_string[VM_EVENT_MAX] = {
	"Power On",				// 0
	"Reset",
	"Reset Complete",
	"BIOS Boot",
	"Guest OS Boot",

	"DOS Boot",				// 5
	"DOS Init, kernel ready",
	"DOS Init, CONFIG.SYS done",
	"DOS Init, shell ready",
	"DOS Init, AUTOEXEC.BAT done",

	"DOS Init, at promot",			// 10
	"DOS exit, begin",
	"DOS exit, kernel exit"
};

VMDispatchState vm_dispatch_state;

const char *GetVMEventName(enum vm_event event) {
	if (event >= VM_EVENT_MAX) return "";
	return VM_EVENT_string[event];
};

void DispatchVMEvent(enum vm_event event) {
	assert(event < VM_EVENT_MAX);

	LOG(LOG_MISC,LOG_DEBUG)("Dispatching VM event %s",GetVMEventName(event));

	vm_dispatch_state.begin_event(event);
	for (std::list<Function_wrapper>::iterator i=vm_event_functions[event].begin();i!=vm_event_functions[event].end();i++) {
		LOG(LOG_MISC,LOG_DEBUG)("Calling event %s handler (%p) '%s'",GetVMEventName(event),(void*)((*i).function),(*i).name.c_str());
		(*i).function(NULL);
	}

	vm_dispatch_state.end_event();
}

Config::~Config() {
	std::list<Section*>::iterator it; // FIXME: You guys do realize C++ STL provides reverse_iterator?

	while ((it=sectionlist.end()) != sectionlist.begin()) {
		it--;
		delete (*it);
		sectionlist.erase(it);
	}
}

Section* Config::GetSection(int index){
	for (it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){
		if (!index--) return (*tel);
	}
	return NULL;
}
//c_str() 2x
Section* Config::GetSection(string const& _sectionname) const{
	for (const_it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){
		if (!strcasecmp((*tel)->GetName(),_sectionname.c_str())) return (*tel);
	}
	return NULL;
}

Section* Config::GetSectionFromProperty(char const * const prop) const{
   	for (const_it tel=sectionlist.begin(); tel!=sectionlist.end(); tel++){
		if ((*tel)->GetPropValue(prop) != NO_SUCH_PROPERTY) return (*tel);
	}
	return NULL;
}


bool Config::ParseConfigFile(char const * const configfilename){
	LOG(LOG_MISC,LOG_DEBUG)("Attempting to load config file #%zu from %s",configfiles.size(),configfilename);

	//static bool first_configfile = true;
	ifstream in(configfilename);
	if (!in) return false;
	const char * settings_type;
	settings_type = (configfiles.size() == 0)? "primary":"additional";
	configfiles.push_back(configfilename);
	
	LOG(LOG_MISC,LOG_NORMAL)("Loading %s settings from config file %s", settings_type,configfilename);

	//Get directory from configfilename, used with relative paths.
	current_config_dir=configfilename;
	std::string::size_type pos = current_config_dir.rfind(CROSS_FILESPLIT);
	if(pos == std::string::npos) pos = 0; //No directory then erase string
	current_config_dir.erase(pos);

	string gegevens;
	Section* currentsection = NULL;
	Section* testsec = NULL;
	while (getline(in,gegevens)) {
		
		/* strip leading/trailing whitespace */
		trim(gegevens);
		if(!gegevens.size()) continue;

		switch(gegevens[0]){
		case '%':
		case '\0':
		case '#':
		case ' ':
		case '\n':
			continue;
			break;
		case '[':
		{
			string::size_type loc = gegevens.find(']');
			if(loc == string::npos) continue;
			gegevens.erase(loc);
			testsec = GetSection(gegevens.substr(1));
			currentsection = testsec; /* NTS: If we don't recognize the section we WANT currentsection == NULL so it has no effect */
			testsec = NULL;
		}
			break;
		default:
			try {
				if(currentsection) currentsection->HandleInputline(gegevens);
			} catch(const char* message) {
				message=0;
				//EXIT with message
			}
			break;
		}
	}
	current_config_dir.clear();//So internal changes don't use the path information
	return true;
}

void Config::ParseEnv(char ** envp) {
	for(char** env=envp; *env;env++) {
		char copy[1024];
		safe_strncpy(copy,*env,1024);
		if(strncasecmp(copy,"DOSBOX_",7))
			continue;
		char* sec_name = &copy[7];
		if(!(*sec_name))
			continue;
		char* prop_name = strrchr(sec_name,'_');
		if(!prop_name || !(*prop_name))
			continue;
		*prop_name++=0;
		Section* sect = GetSection(sec_name);
		if(!sect)
			continue;
		sect->HandleInputline(prop_name);
	}
}

bool CommandLine::FindExist(char const * const name,bool remove) {
	cmd_it it;
	if (!(FindEntry(name,it,false))) return false;
	if (remove) cmds.erase(it);
	return true;
}

bool CommandLine::FindHex(char const * const name,int & value,bool remove) {
	cmd_it it,it_next;
	if (!(FindEntry(name,it,true))) return false;
	it_next=it;it_next++;
	sscanf((*it_next).c_str(),"%X",&value);
	if (remove) cmds.erase(it,++it_next);
	return true;
}

bool CommandLine::FindInt(char const * const name,int & value,bool remove) {
	cmd_it it,it_next;
	if (!(FindEntry(name,it,true))) return false;
	it_next=it;it_next++;
	value=atoi((*it_next).c_str());
	if (remove) cmds.erase(it,++it_next);
	return true;
}

bool CommandLine::FindString(char const * const name,std::string & value,bool remove) {
	cmd_it it,it_next;
	if (!(FindEntry(name,it,true))) return false;
	it_next=it;it_next++;
	value=*it_next;
	if (remove) cmds.erase(it,++it_next);
	return true;
}

bool CommandLine::FindCommand(unsigned int which,std::string & value) {
	if (which<1) return false;
	if (which>cmds.size()) return false;
	cmd_it it=cmds.begin();
	for (;which>1;which--) it++;
	value=(*it);
	return true;
}

bool CommandLine::FindEntry(char const * const name,cmd_it & it,bool neednext) {
	for (it=cmds.begin();it!=cmds.end();it++) {
		const char *d = (*it).c_str();

		/* HACK: If the search string starts with -, it's a switch,
		 *       so adjust pointers so that it matches --switch or -switch */
		if (*name == '-' && d[0] == '-' && d[1] == '-') d++;

		if (!strcasecmp(d,name)) {
			cmd_it itnext=it;itnext++;
			if (neednext && (itnext==cmds.end())) return false;
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringBegin(char const* const begin,std::string & value, bool remove) {
	size_t len = strlen(begin);
	for (cmd_it it=cmds.begin();it!=cmds.end();it++) {
		if (strncmp(begin,(*it).c_str(),len)==0) {
			value=((*it).c_str() + len);
			if (remove) cmds.erase(it);
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringRemain(char const * const name,std::string & value) {
	cmd_it it;value="";
	if (!FindEntry(name,it)) return false;
	it++;
	for (;it!=cmds.end();it++) {
		value+=" ";
		value+=(*it);
	}
	return true;
}

/* Only used for parsing command.com /C 
 * Allowing /C dir and /Cdir
 * Restoring quotes back into the commands so command /C mount d "/tmp/a b" works as intended
 */
bool CommandLine::FindStringRemainBegin(char const * const name,std::string & value) {
	cmd_it it;value="";
	if (!FindEntry(name,it)) {
		size_t len = strlen(name);
			for (it=cmds.begin();it!=cmds.end();it++) {
				if (strncasecmp(name,(*it).c_str(),len)==0) {
					std::string temp = ((*it).c_str() + len);
					//Restore quotes for correct parsing in later stages
					if(temp.find(" ") != std::string::npos)
						value = std::string("\"") + temp + std::string("\"");
					else
						value = temp;
					break;
				}
			}
		if( it == cmds.end()) return false;
	}
	it++;
	for (;it!=cmds.end();it++) {
		value += " ";
		std::string temp = (*it);
		if(temp.find(" ") != std::string::npos)
			value += std::string("\"") + temp + std::string("\"");
		else
			value += temp;
	}
	return true;
}

bool CommandLine::GetStringRemain(std::string & value) {
	if(!cmds.size()) return false;
		
	cmd_it it=cmds.begin();value=(*it++);
	for(;it != cmds.end();it++) {
		value+=" ";
		value+=(*it);
	}
	return true;
}
		

unsigned int CommandLine::GetCount(void) {
	return (unsigned int)cmds.size();
}

bool CommandLine::NextOptArgv(std::string &argv) {
	argv.clear();

	/* no argv to return if we're doing single-char GNU switches */
	if (!opt_gnu_getopt_singlechar.empty()) return false;

	if (opt_scan == cmds.end()) return false;
	argv = *opt_scan;
	if (opt_eat_argv) opt_scan = cmds.erase(opt_scan);
	else opt_scan++;
	return true;
}

void CommandLine::ChangeOptStyle(enum opt_style opt_style) {
	this->opt_style = opt_style;
}

bool CommandLine::BeginOpt(bool eat_argv) {
	opt_gnu_getopt_singlechar.clear();
	opt_scan = cmds.begin();
	if (opt_scan == cmds.end()) return false;
	opt_eat_argv = eat_argv;
	return true;
}

bool CommandLine::GetOptGNUSingleCharCheck(std::string &name) {
	char c;

	/* return another char, skipping spaces or invalid chars */
	name.clear();
	while (!opt_gnu_getopt_singlechar.empty()) {
		c = opt_gnu_getopt_singlechar.at(0);
		opt_gnu_getopt_singlechar = opt_gnu_getopt_singlechar.substr(1);
		if (c <= ' ' || c > 126) continue;

		name = c;
		return true;
	}

	return false;
}

bool CommandLine::GetOpt(std::string &name) {
	name.clear();

	/* if we're still doing GNU getopt single-char switches, then parse another and return */
	if (GetOptGNUSingleCharCheck(name))
		return true;

	while (opt_scan != cmds.end()) {
		std::string &argv = *opt_scan;
		const char *str = argv.c_str();

		if ((opt_style == CommandLine::either || opt_style == CommandLine::dos) && *str == '/') {
			/* MS-DOS style /option. Example: /A /OPT /HAX /BLAH */
			name = str+1; /* copy to caller minus leaking slash, then erase/skip */
			if (opt_eat_argv) opt_scan = cmds.erase(opt_scan);
			else opt_scan++;
			return true;
		}
		else if ((opt_style == CommandLine::either || opt_style == CommandLine::gnu || opt_style == CommandLine::gnu_getopt) && *str == '-') {
			str++; /* step past '-' */
			if (str[0] == '-' && str[1] == 0) { /* '--' means to stop parsing */
				opt_scan = cmds.end();
				if (opt_eat_argv) opt_scan = cmds.erase(opt_scan);
				break;
			}

			if (opt_style == CommandLine::gnu_getopt) {
				/* --switch => "switch"
				 * -switch => -s -w -i -t -c -h */
				if (*str == '-') {
					str++;
				}
				else {
					/* assign to single-char parse then eat the argv */
					opt_gnu_getopt_singlechar = str;
					if (opt_eat_argv) opt_scan = cmds.erase(opt_scan);
					else opt_scan++;

					/* if we parse a single-char switch, great */
					if (GetOptGNUSingleCharCheck(name))
						return true;

					continue; /* if we're here then there was nothing to parse, continue */
				}
			}
			else {
				/* -switch and --switch mean the same thing */
				if (*str == '-') str++;
			}

			name = str; /* copy to caller, then erase/skip */
			if (opt_eat_argv) opt_scan = cmds.erase(opt_scan);
			else opt_scan++;
			return true;
		}
		else {
			opt_scan++;
		}
	}

	return false;
}

void CommandLine::EndOpt() {
	opt_scan = cmds.end();
}

void CommandLine::FillVector(std::vector<std::string> & vector) {
	for(cmd_it it=cmds.begin(); it != cmds.end(); it++) {
		vector.push_back((*it));
	}
	// add back the \" if the parameter contained a space
	for(Bitu i = 0; i < vector.size(); i++) {
		if(vector[i].find(' ') != std::string::npos) {
			vector[i] = "\""+vector[i]+"\"";
		}
	}
}

int CommandLine::GetParameterFromList(const char* const params[], std::vector<std::string> & output) {
	// return values: 0 = P_NOMATCH, 1 = P_NOPARAMS
	// TODO return nomoreparams
	int retval = 1;
	output.clear();
	enum {
		P_START, P_FIRSTNOMATCH, P_FIRSTMATCH
	} parsestate = P_START;
	cmd_it it = cmds.begin();
	while(it!=cmds.end()) {
		bool found = false;
		for(Bitu i = 0; params[i] != NULL; i++) {
			if (*params[i] == 0) {
				LOG_MSG("FIXME: GetParameterFromList: terminating params[] with \"\" is deprecated. Please terminate the param list with NULL");
				break;
			}

			if (!strcasecmp((*it).c_str(),params[i])) {
				// found a parameter
				found = true;
				switch(parsestate) {
				case P_START:
					retval = i+2;
					parsestate = P_FIRSTMATCH;
					break;
				case P_FIRSTMATCH:
				case P_FIRSTNOMATCH:
					return retval;
				}
			}
		}
		if(!found) 
			switch(parsestate) {
			case P_START:
				retval = 0; // no match
				parsestate = P_FIRSTNOMATCH;
				output.push_back(*it);
				break;
			case P_FIRSTMATCH:
			case P_FIRSTNOMATCH:
				output.push_back(*it);
				break;
			}
		cmd_it itold = it;
		it++;
		cmds.erase(itold);

	}
	
	return retval;
}


CommandLine::CommandLine(int argc,char const * const argv[],enum opt_style opt) {
	if (argc>0) {
		file_name=argv[0];
	}
	int i=1;
	opt_style = opt;
	while (i<argc) {
		cmds.push_back(argv[i]);
		i++;
	}
}

Bit16u CommandLine::Get_arglength() {
	if(cmds.empty()) return 0;
	Bit16u i=1;
	for(cmd_it it=cmds.begin();it != cmds.end();it++) 
		i+=(*it).size() + 1;
	return --i;
}


CommandLine::CommandLine(char const * const name,char const * const cmdline,enum opt_style opt) {
	if (name) file_name=name;
	/* Parse the cmds and put them in the list */
	bool inword,inquote;char c;
	inword=false;inquote=false;
	std::string str;
	const char * c_cmdline=cmdline;
	opt_style = opt;
	while ((c=*c_cmdline)!=0) {
		if (inquote) {
			if (c!='"') str+=c;
			else {
				inquote=false;
				cmds.push_back(str);
				str.erase();
			}
		}else if (inword) {
			if (c!=' ') str+=c;
			else {
				inword=false;
				cmds.push_back(str);
				str.erase();
			}
		} 
		else if (c=='"') { inquote=true;}
		else if (c!=' ') { str+=c;inword=true;}
		c_cmdline++;
	}
	if (inword || inquote) cmds.push_back(str);
}

void CommandLine::Shift(unsigned int amount) {
	while(amount--) {
		file_name = cmds.size()?(*(cmds.begin())):"";
		if(cmds.size()) cmds.erase(cmds.begin());
	}
}
