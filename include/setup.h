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


#ifndef DOSBOX_SETUP_H
#define DOSBOX_SETUP_H

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#pragma warning ( disable : 4290 )
#endif


#ifndef CH_LIST
#define CH_LIST
#include <list>
#endif

#ifndef CH_VECTOR
#define CH_VECTOR
#include <vector>
#endif

#ifndef CH_STRING
#define CH_STRING
#include <string>
#endif

#ifndef CH_CSTDIO
#define CH_CSTDIO
#include <cstdio>
#endif


class Hex {
private:
	int _hex;
public:
	Hex(int in):_hex(in) { };
	Hex():_hex(0) { };
	bool operator==(Hex const& other) {return _hex == other._hex;}
	operator int () const { return _hex; }
   
};

class Value {
/* 
 * Multitype storage container that is aware of the currently stored type in it.
 * Value st = "hello";
 * Value in = 1;
 * st = 12 //Exception
 * in = 12 //works
 */
private:
	Hex _hex;
	bool _bool;
	int _int;
	std::string* _string;
	double _double;
public:
	class WrongType { }; // Conversion error class
	enum Etype { V_NONE, V_HEX, V_BOOL, V_INT, V_STRING, V_DOUBLE,V_CURRENT} type;
	
	/* Constructors */
	Value()                      :_string(0),   type(V_NONE)                  { };
	Value(Hex in)                :_hex(in),     type(V_HEX)                   { };
	Value(int in)                :_int(in),     type(V_INT)                   { };
	Value(bool in)               :_bool(in),    type(V_BOOL)                  { };
	Value(double in)             :_double(in),  type(V_DOUBLE)                { };
	Value(std::string const& in) :_string(new std::string(in)),type(V_STRING) { };
	Value(char const * const in) :_string(new std::string(in)),type(V_STRING) { };
	Value(Value const& in):_string(0) {plaincopy(in);}
	~Value() { destroy();};
	Value(std::string const& in,Etype _t) :_hex(0),_bool(false),_int(0),_string(0),_double(0),type(V_NONE) {SetValue(in,_t);}

	/* Assigment operators */
	Value& operator= (Hex in) throw(WrongType)                { return copy(Value(in));}
	Value& operator= (int in) throw(WrongType)                { return copy(Value(in));}
	Value& operator= (bool in) throw(WrongType)               { return copy(Value(in));}
	Value& operator= (double in) throw(WrongType)             { return copy(Value(in));}
	Value& operator= (std::string const& in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (char const * const in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (Value const& in) throw(WrongType)       { return copy(Value(in));}

	bool operator== (Value const & other);
	operator bool () const throw(WrongType);
	operator Hex () const throw(WrongType);
	operator int () const throw(WrongType);
	operator double () const throw(WrongType);
	operator char const* () const throw(WrongType);
	bool SetValue(std::string const& in,Etype _type = V_CURRENT) throw(WrongType);
	std::string ToString() const;

private:
	void destroy() throw();
	Value& copy(Value const& in) throw(WrongType);
	void plaincopy(Value const& in) throw();
	bool set_hex(std::string const& in);
	bool set_int(std::string const&in);
	bool set_bool(std::string const& in);
	void set_string(std::string const& in);
	bool set_double(std::string const& in);
};

class Property {
public:
	struct Changeable { enum Value {Always, WhenIdle,OnlyAtStart};};
	const std::string propname;

	Property(std::string const& _propname, Changeable::Value when):propname(_propname),is_modified(false),change(when) { use_global_config_str=false; }
	void Set_values(const char * const * in);
	void Set_help(std::string const& str);
	char const* Get_help();
	virtual	bool SetValue(std::string const& str)=0;
	Value const& GetValue() const { return value;}
	Value const& Get_Default_Value() const { return default_value; }
	//CheckValue returns true  if value is in suggested_values;
	//Type specific properties are encouraged to override this and check for type
	//specific features.
	virtual bool CheckValue(Value const& in, bool warn);
	//Set interval value to in or default if in is invalid. force always sets the value.
	bool SetVal(Value const& in, bool forced,bool warn=true,bool init=false) {
		if(forced || CheckValue(in,warn)) {value = in; is_modified = !init; return true;} else { value = default_value; is_modified = false; return false;}}
	virtual ~Property(){ } 
	virtual const std::vector<Value>& GetValues() const;
	Value::Etype Get_type(){return default_value.type;}
	Changeable::Value getChange() {return change;}
	bool modified() const { return is_modified; };

protected:
	Value value;
	bool is_modified;
	std::vector<Value> suggested_values;
	typedef std::vector<Value>::iterator iter;
	Value default_value;
	const Changeable::Value change;
	bool use_global_config_str;
	std::string help_string;
};

class Prop_int:public Property {
public:
	Prop_int(std::string const& _propname,Changeable::Value when, int _value)
		:Property(_propname,when) { 
		default_value = value = _value;
		min = max = -1;
	}
	Prop_int(std::string const&  _propname,Changeable::Value when, int _min,int _max,int _value)
		:Property(_propname,when) { 
		default_value = value = _value;
		min = _min;
		max = _max;
	}
	int getMin() { return min;}
	int getMax() { return max;}
	void SetMinMax(Value const& min,Value const& max) {this->min = min; this->max=max;}
	bool SetValue(std::string const& in);
	virtual ~Prop_int(){ }
	virtual bool CheckValue(Value const& in, bool warn);
private:
	Value min,max;
};

class Prop_double:public Property {
public:
	Prop_double(std::string const & _propname, Changeable::Value when, double _value)
		:Property(_propname,when){
		default_value = value = _value;
	}
	bool SetValue(std::string const& input);
	virtual ~Prop_double(){ }
};

class Prop_bool:public Property {
public:
	Prop_bool(std::string const& _propname, Changeable::Value when, bool _value)
		:Property(_propname,when) { 
		default_value = value = _value;
	}
	bool SetValue(std::string const& in);
	virtual ~Prop_bool(){ }
};

class Prop_string:public Property{
public:
	Prop_string(std::string const& _propname, Changeable::Value when, char const * const _value)
		:Property(_propname,when) { 
		default_value = value = _value;
	}
	bool SetValue(std::string const& in);
	virtual bool CheckValue(Value const& in, bool warn);
	virtual ~Prop_string(){ }
};
class Prop_path:public Prop_string{
public:
	std::string realpath;
	Prop_path(std::string const& _propname, Changeable::Value when, char const * const _value)
		:Prop_string(_propname,when,_value) { 
		default_value = value = _value;
		realpath = _value;
	}
	bool SetValue(std::string const& in);
	virtual ~Prop_path(){ }
};

class Prop_hex:public Property {
public:
	Prop_hex(std::string const& _propname, Changeable::Value when, Hex _value)
		:Property(_propname,when) { 
		default_value = value = _value;
	}
	bool SetValue(std::string const& in);
	virtual ~Prop_hex(){ }
};

class Section;

typedef void (*SectionFunction)(Section*);

/* Wrapper class around startup and shutdown functions. the variable
 * canchange indicates it can be called on configuration changes */
struct Function_wrapper {
	SectionFunction function;
	bool canchange;
	Function_wrapper(SectionFunction const _fun,bool _ch){
		function=_fun;
		canchange=_ch;
	}
};

#define NO_SUCH_PROPERTY "PROP_NOT_EXIST"
class Section {
private:
	std::list<Function_wrapper> initfunctions;
	std::list<Function_wrapper> destroyfunctions;
	std::string sectionname;
public:
	Section(std::string const& _sectionname):sectionname(_sectionname) {  }

	void AddInitFunction(SectionFunction func,bool canchange=false);
	void AddDestroyFunction(SectionFunction func,bool canchange=false);
	void ExecuteInit(bool initall=true);
	void ExecuteDestroy(bool destroyall=true);
	const char* GetName() const {return sectionname.c_str();}

	virtual std::string GetPropValue(std::string const& _property) const =0;
	virtual bool HandleInputline(std::string const& _line)=0;
	virtual void PrintData(FILE* outfile,bool everything=false) = 0;
	virtual ~Section() { /*Children must call executedestroy ! */ }
};

/* list of functions to call (in list order) when DOSBox-X exits.
 * use AddExitFunction() to add your function.
 * NOTE: AddExitFunction() adds your function to the back of the list,
 *       First-In-Last-Out order, so that exit callbacks added by init
 *       code are called in the opposite order from initialization
 *       (i.e. we want high-level stuff to cleanup first and low level
 *       stuff like logging to cleanup last). */
extern std::list<Function_wrapper> exitfunctions;
void AddExitFunction(SectionFunction func,bool canchange=false);

/* array of list of functions to call for various virtual machine events */
enum {
	VM_EVENT_POWERON=0,			// emulation has started to power on hardware. it is safe to connect I/O, memory, IRQ resources, etc. to the bus. BIOS not initialized yet.
	VM_EVENT_RESET,				// reset signal (at the hardware level), whether by the keyboard controller, reset button, etc.
	VM_EVENT_BIOS_BOOT,			// BIOS in the boot stage. usually leads to DOS kernel init or guest OS boot.
	VM_EVENT_GUEST_OS_BOOT,			// BIOS or DOS kernel (BOOT command) is running a guest OS. just after loading boot sector into memory but before executing it.
	VM_EVENT_DOS_BOOT,			// emulation has decided to boot the built-in DOS kernel. just prior to starting the DOS kernel.
	VM_EVENT_DOS_INIT_KERNEL_READY,		// DOS kernel init. Prior to CONFIG.SYS handling.
	VM_EVENT_DOS_INIT_CONFIG_SYS_DONE,	// DOS kernel init. After CONFIG.SYS handling, all devices inited.
	VM_EVENT_DOS_INIT_SHELL_READY,		// DOS kernel init. After COMMAND.COM initialization, before AUTOEXEC.BAT execution.
	VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE,	// DOS kernel init. COMMAND.COM just finished AUTOEXEC.BAT.
	VM_EVENT_DOS_INIT_AT_PROMPT,		// DOS kernel init complete. After this event, the user is immediately given the DOS prompt.
	VM_EVENT_DOS_EXIT_BEGIN,		// DOS kernel is just starting to exit (user used BOOT command)
	VM_EVENT_DOS_EXIT_KERNEL,		// DOS kernel has just finished exiting

	VM_EVENT_MAX
};

extern std::list<Function_wrapper> vm_event_functions[VM_EVENT_MAX];
void AddVMEventFunction(unsigned int event,SectionFunction func,bool canchange=false);
void DispatchVMEvent(unsigned int event);

class Prop_multival;
class Prop_multival_remain;
class Section_prop:public Section {
private:
	std::list<Property*> properties;
	typedef std::list<Property*>::iterator it;
	typedef std::list<Property*>::const_iterator const_it;

public:
	Section_prop(std::string const&  _sectionname):Section(_sectionname){}
	Prop_int* Add_int(std::string const& _propname, Property::Changeable::Value when, int _value=0);
	Prop_string* Add_string(std::string const& _propname, Property::Changeable::Value when, char const * const _value=NULL);
	Prop_path* Add_path(std::string const& _propname, Property::Changeable::Value when, char const * const _value=NULL);
	Prop_bool*  Add_bool(std::string const& _propname, Property::Changeable::Value when, bool _value=false);
	Prop_hex* Add_hex(std::string const& _propname, Property::Changeable::Value when, Hex _value=0);
	Prop_double* Add_double(std::string const& _propname, Property::Changeable::Value when, double _value=0.0);   
	Prop_multival *Add_multi(std::string const& _propname, Property::Changeable::Value when,std::string const& sep);
	Prop_multival_remain *Add_multiremain(std::string const& _propname, Property::Changeable::Value when,std::string const& sep);

	Property* Get_prop(int index);
	int Get_int(std::string const& _propname) const;
	const char* Get_string(std::string const& _propname) const;
	bool Get_bool(std::string const& _propname) const;
	Hex Get_hex(std::string const& _propname) const;
	double Get_double(std::string const& _propname) const;
	Prop_path* Get_path(std::string const& _propname) const;
	Prop_multival* Get_multival(std::string const& _propname) const;
	Prop_multival_remain* Get_multivalremain(std::string const& _propname) const;
	virtual bool HandleInputline(std::string const& gegevens);
	virtual void PrintData(FILE* outfile,bool everything=false);
	virtual std::string GetPropValue(std::string const& _property) const;
	//ExecuteDestroy should be here else the destroy functions use destroyed properties
	virtual ~Section_prop();
};

class Prop_multival:public Property{
protected:
	Section_prop* section;
	std::string seperator;
	void make_default_value();
public:
	Prop_multival(std::string const& _propname, Changeable::Value when,std::string const& sep):Property(_propname,when), section(new Section_prop("")),seperator(sep) {
		default_value = value = "";
	}
	Section_prop *GetSection() { return section; }
	const Section_prop *GetSection() const { return section; }
	virtual bool SetValue(std::string const& input,bool init);
	virtual bool SetValue(std::string const& input) { return SetValue(input,/*init*/false); };
	virtual const std::vector<Value>& GetValues() const;
	virtual ~Prop_multival() { if (section != NULL) { delete section; } }
}; //value bevat totale string. setvalue zet elk van de sub properties en checked die.

class Prop_multival_remain:public Prop_multival{
public:
	Prop_multival_remain(std::string const& _propname, Changeable::Value when,std::string const& sep):Prop_multival(_propname,when,sep){ }

	virtual bool SetValue(std::string const& input,bool init);
	virtual bool SetValue(std::string const& input) { return SetValue(input,/*init*/false); };
};

   
class Section_line: public Section{
public:
	Section_line(std::string const& _sectionname):Section(_sectionname){}
	virtual ~Section_line() { ExecuteDestroy(true); }
	virtual bool HandleInputline(std::string const& gegevens);
	virtual void PrintData(FILE* outfile,bool everything=false);
	virtual std::string GetPropValue(std::string const& _property) const;
	std::string data;
};

class Module_base {
	/* Base for all hardware and software "devices" */
protected:
	Section* m_configuration;
public:
	Module_base(Section* configuration){m_configuration=configuration;};
//	Module_base(Section* configuration, SaveState* state) {};
	virtual ~Module_base(){/*LOG_MSG("executed")*/;};//Destructors are required
	/* Returns true if succesful.*/
	virtual bool Change_Config(Section* /*newconfig*/) {return false;} ;
};
#endif
