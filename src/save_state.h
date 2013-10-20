#ifndef SAVE_STATE_H_INCLUDED
#define SAVE_STATE_H_INCLUDED

#include <sstream>
#include <map>
#include <algorithm>
#include <functional>
#include <vector>


#define WRITE_POD(x,y) \
	stream.write(reinterpret_cast<const char*>( (x) ), sizeof( (y) ) );

#define WRITE_POD_SIZE(x,y) \
	stream.write(reinterpret_cast<const char*>( (x) ), (y) );

#define READ_POD(x,y) \
	stream.read(reinterpret_cast<char*>( (x) ), sizeof( (y) ) );

#define READ_POD_SIZE(x,y) \
	stream.read(reinterpret_cast<char*>( (x) ), (y) );



class SaveState
{
public:
    static SaveState& instance();

    typedef std::string Error;
    static const size_t SLOT_COUNT = 10; //slot: [0,...,SLOT_COUNT - 1]

    void save   (size_t slot);       //throw (Error)
    void load   (size_t slot) const; //throw (Error)
    bool isEmpty(size_t slot) const;

    //initialization: register relevant components on program startup
    struct Component
    {
        virtual void getBytes(std::ostream& stream) = 0;
        virtual void setBytes(std::istream& stream) = 0;
    };

    void registerComponent(const std::string& uniqueName, Component& comp); //comp must have global lifetime!

private:
    SaveState() {}
    SaveState(const SaveState&);
    SaveState& operator=(const SaveState&);

    class RawBytes
    {
    public:
        RawBytes() : dataExists(false), isCompressed(false) {}
        void set(const std::string& stream);
        std::string get() const; //throw (Error)
        void compress() const;   //throw (Error)
        bool dataAvailable() const;
    private:
        bool dataExists; //determine whether set method (even with empty string) was called
        mutable bool isCompressed; //design for logical not binary const
        mutable std::string bytes; //
    };

    struct CompData
    {
        CompData(Component& cmp) : comp(cmp), rawBytes(SLOT_COUNT) {}
        Component& comp;
        std::vector<RawBytes> rawBytes;
    };

    typedef std::map<std::string, CompData> CompEntry;
    CompEntry components;
};


//some helper functions
template <class T>
void writePOD(std::ostream& stream, const T& data);

template <class T>
void readPOD(std::istream& stream, T& data);

void writeString(std::ostream& stream, const std::string& data);
void readString(std::istream& stream, std::string& data);

//Implementation of SaveState::Component for saving POD types only
class SerializeGlobalPOD : public SaveState::Component
{
public:
    SerializeGlobalPOD(const std::string& compName)
    {
    }

    template <class T>
    void registerPOD(T& pod) //register POD for serializatioin
    {
    }

protected:
    virtual void getBytes(std::ostream& stream)
    {
    }

    virtual void setBytes(std::istream& stream)
    {
    }

private:
    struct POD
    {
        template <class T>
        POD(T& pod) : address(&pod), size(sizeof(T)) {}
        void* address;
        size_t size;
    };

    struct WriteGlobalPOD : public std::binary_function<std::ostream*, POD, void>
    {
        void operator()(std::ostream* stream, const POD& data) const
        {
        }
    };

    struct ReadGlobalPOD : public std::binary_function<std::istream*, POD, void>
    {
        void operator()(std::istream* stream, const POD& data) const
        {
        }
    };

    std::vector<POD> podRef;
};

//---------------- inline implementation -------------------------
template <class T>
inline
void writePOD(std::ostream& stream, const T& data)
{
}

template <class T>
inline
void readPOD(std::istream& stream, T& data)
{
}

inline
void writeString(std::ostream& stream, const std::string& data)
{
}

inline
void readString(std::istream& stream, std::string& data)
{
}

#endif //SAVE_STATE_H_INCLUDED
