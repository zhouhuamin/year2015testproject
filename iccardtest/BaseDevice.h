#if !defined MODULE_BASE_INCLUDE
#define MODULE_BASE_INCLUDE



#define MODULE_ICCARD_READER  (0X1000)
#define SYS_MSG_MODULE_ICCARD_READER_READCOMPLETE  (0X1000+1)

struct DeviceInfo
{
    int com_port;
    int baud_date;
};

struct ModuleInfo
{
    char module_name[64];
    char factory_name[128];
};


typedef void (*_READ_DATA_CALLBACK_) (unsigned char* read_data, void* user_data);
typedef void (*_ERROR_STATE_CALLBACK_) (int error_state, void* user_data);


class CBaseDevice
{
public:

    CBaseDevice()
    {
    };

    virtual ~CBaseDevice()
    {
    };


    virtual int init_device(DeviceInfo device_info)
    {
        return 0;
    }

  
    virtual int get_module_info(ModuleInfo& moduleinfo)
    {
        return 0;
    };



    virtual int start_work()
    {
        return 0;
    };


    virtual int pause(bool is_stop)
    {
        return 0;
    };


    virtual int stop_work()
    {
        return 0;
    };

 
    virtual int read_data()
    {
        return 0;
    };


  
    virtual int begin_test_device()
    {
        return 0;
    };

  
    virtual int end_test_device()
    {
        return 0;
    };

 
    virtual int AutoDectectDevice()
    {
        return 0;
    };

  
    virtual int SetRunState()
    {
        return 0;
    };


    virtual int reset_device()
    {
        return 0;
    };

 
    virtual int get_device_state()
    {
        return 0;
    };

   
    virtual int set_device_state()
    {
        return 0;
    };

   
    virtual int close_device()
    {
        return 0;
    };

    
    virtual int begin_test()
    {
        return 0;
    };


    virtual int end_test()
    {
        return 0;
    };

    virtual int SetReadDataCallback(_READ_DATA_CALLBACK_ pReadDataCallback,void* pUserData)
    {
        return 0;
    };

    virtual int SetErrorStateCallback(_ERROR_STATE_CALLBACK_ pErrorStateCallback,void* pUserData)
    {
        return 0;
    };

    //CONTROL INTERFACE
    virtual int led_show_pic(char* pic_screen_code)
    {
        return 0;
    };

    virtual int led_show_text(char* screen_text)
    {
        return 0;
    };


    virtual int play_sound(char* sound_code)
    {
        return 0;
    };

    virtual int give_alarm(int time_span)
    {
        return 0;
    };



    virtual int lcd_show_text(char* conta_idf, char* conta_idb, char* conta_damage_f, char* conta_damage_b)
    {
        return 0;
    };


    virtual int print_ticket(char* chartoprint)
    {
        return 0;
    };


    //IO PORT INTERFACE
    virtual unsigned char ReadPortByteData(unsigned short wChannel)
    {
        return 0;
    }

    virtual unsigned char WritePortByteData(unsigned short wChannel, unsigned char wData)
    {
        return 0;
    }

    virtual unsigned char WritePortDwordData(unsigned short wChannel, unsigned int dwData)
    {
        return 0;
    }

    virtual unsigned char ReadPortDwordData(unsigned short wChannel)
    {
        return 0;
    }

    virtual unsigned char WriteIOBit(unsigned short wChannel, unsigned short wBit, unsigned short wState)
    {
        return 0;
    }


};



// the types of the class factories
typedef CBaseDevice* create_t();
typedef void destroy_t(CBaseDevice*);


#endif

