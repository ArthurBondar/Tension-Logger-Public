/*




*/


#include "Sensor.h"


uint8_t Sensor::init(unsigned long baud, bool date_en)
{
    Serial2.begin(baud, SERIAL_8N1, RXD2, TXD2);
    if(!Serial2) {
        log_e("failed to init");
        return FAILED;
    }
    //Serial2.setRxBufferSize( (size_t) SERIAL_BUFF );
    this->_date_en = date_en;
    this->_data_i = 0;
    this->resetLine();
    log_i("initialized");
    this->_initialized=true;
    return SUCCESS;
}

void Sensor::dumpData(void)
{
    for (int i = 0; i < this->_data_i; i++) {
        log_i("[%d] time: %d-%02d-%02d %02d:%02d:%02d\t tension:%.1f \tunit:%s \tpeak: %.1f \tunit: %s",
        i,
        this->_data[i].timestamp.tm_year + 1900,
        this->_data[i].timestamp.tm_mon + 1,
        this->_data[i].timestamp.tm_mday,
        this->_data[i].timestamp.tm_hour,
        this->_data[i].timestamp.tm_min,
        this->_data[i].timestamp.tm_sec,
        this->_data[i].tension, 
        this->_data[i].unit1, 
        this->_data[i].peak_tension, 
        this->_data[i].unit2
        );
    }
    log_i("\n");
}

uint8_t Sensor::getDataIndex(void)
{
    return this->_data_i;
}

void Sensor::clearDataIndex(void)
{
    this->_data_i = 0;
}

// [date]<tb>[time]<tb><sp>[val1]<tb>[unit1]<tb><sp>[val2]<tb>[unit2]<tb><sp>[0]
uint8_t Sensor::_parseData(void)
{
    time_t rawTime;
    char unit1[UNITS_LEN]= "NaN", unit2[UNITS_LEN] = "NaN";
    float val1 = 0.0, val2 = 0.0;

    char *tok = strtok(this->_line, "\t");
    // discard first two parameters - date and time
    if(this->_date_en == true) {
        strtok(NULL, "\t");
        tok = strtok(NULL, "\t");
    }

    // val 1
    if (tok == NULL) {
        log_e("parsing error");
        return FAILED;
    }
    val1 = atof(tok);

    // unit 1
    tok = strtok(NULL, "\t");
    if (tok == NULL) {
        log_e("parsing error");
        return FAILED;
    }
    strncpy(unit1, tok, sizeof(unit1));

    // get second param
    if(this->_mode == 3) 
    {
        tok = strtok(NULL, "\t");
        if (tok == NULL) {
            log_e("parsing error");
            return FAILED;
        }
        val2 = atof(tok);
        tok = strtok(NULL, "\t");
        if (tok == NULL) {
            log_e("parsing error");
            return FAILED;
        }
        strncpy(unit2, tok, sizeof(unit2));
    }

    // in mode 2, first param is peak, second param non existant
    if(this->_mode == 2) {
        val2 = val1;
        val1 = 0.0;
    }

    if ( this->_data_i > DATA_POINTS ) {
        log_w("data points overflow, clear buffer");
        return FAILED;
    }

    // write data into the structure
    time( &rawTime );
    this->_data[this->_data_i].timestamp = *(localtime(&rawTime));
    //log_i("\ntime = %s\t val1=[%f]\t unit1=[%s]\t val1=[%f]\t unit2=[%s]", asctime(&this->_data[this->_data_i].timestamp), val1, unit1, val2, unit2);
    this->_data[this->_data_i].tension = val1;
    this->_data[this->_data_i].peak_tension = val2;
    strncpy(this->_data[this->_data_i].unit1, unit1, sizeof(this->_data[this->_data_i].unit1));
    strncpy(this->_data[this->_data_i].unit2, unit2, sizeof(this->_data[this->_data_i].unit2));
    this->_data_i++;

}


//pdMS_TO_TICKS(1000)
uint8_t Sensor::readSerial( void )
{
    if (Serial2.available() && this->_line_i < LINE_BUFF) 
    {
        char ch = (char) Serial2.read();
        this->_line[this->_line_i] = ch;
        if(this->_line_i > 1) {
            // Check for end of line sequence - <tab><space><0> 9,20,30
            if( this->_line[this->_line_i-2] == '\t' && this->_line[this->_line_i-1] == ' ' && this->_line[this->_line_i] == '0' )
            {
                //log_i("%s", this->_line);
                this->_parseData();
                this->resetLine();
                return SUCCESS;
            }
        }
        this->_line_i++;
        return SUCCESS;
    } else {
        return NO_SER_DATA;
    }
}

char* Sensor::getRawData()
{
    return this->_line;
}

void Sensor::resetLine()
{
    memset(this->_line, 0, LINE_BUFF);
    this->_line_i = 0;
    Serial2.flush();
}

//pdMS_TO_TICKS(5000)

uint8_t Sensor::setBaud(unsigned long baud)
{
    uint32_t check = 0;
    Serial2.updateBaudRate(baud);
    check = Serial2.baudRate();
    if (check != baud) {
        log_e("buadrate check failed");
        return FAILED;
    }
    else return SUCCESS;
}

uint8_t Sensor::getMode(void)
{
    return this->_mode;
}

void Sensor::setMode(uint8_t mode)
{
    if(mode > 0 && mode < 6)
        this->_mode = mode;
}

uint8_t Sensor::deinit()
{
    Serial2.end();
    this->_initialized=false;
    return SUCCESS;
}
