/*
NNS @ 2018
nns-freeplay-adc-backlight-daemon
Change backlight based on ADC input
*/
const char programversion[]="0.1a";

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>



int debug=1; //program is in debug mode, 0=no 1=full
#define debug_print(fmt, ...) do { if (debug) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while (0) //Flavor: print advanced debug to stderr

int i2c_bus=-1;									//i2c bus id
char i2c_path[PATH_MAX];				//path to i2c bus
int i2c_addr=0x48;							//i2c device adress
char setPCA9633_path[PATH_MAX];					//path where setPCA9633 program located
char setPCA9633_command[PATH_MAX];			//setPCA9633 command
int update_interval=250;								//update interval in msec

bool i2c_addr_valid=false;			//i2c device adress is valid
int i2c_handle;									//i2c handle io
FILE *temp_filehandle;			//file handle to get cpu temp/usage
char i2c_buffer[10]={0};				//i2c data buffer
int i2c_retry=0;								//reading retry if failure

int adc_raw_value=0;							//adc value
int adc_last_value=-1;						//adc last value
bool adc_reverse=false;						//reverse adc value
int adc_low_value=0;							//adc min value
int adc_high_value=4096;					//adc max value
bool test_mode=false;							//test mode
int adc_debug_low_value=-1;				//adc debug min value
int adc_debug_high_value=-1;			//adc debug max value

int brightness_value=255;					//brightness value

bool MCP3021A_detected=false;			//chip detected bool


int nns_map_int(int x,int in_min,int in_max,int out_min,int out_max){
  if(x<in_min){return out_min;}
  if(x>in_max){return out_max;}
  return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;
}

void show_usage(void){
	printf(
"Example : ./nns-freeplay-adc-backlight-daemon -pin 31 -interval 200 -noinput -debug 1\n"
"Version: %s\n"
"Options:\n"
"\t-i2cbus, I2C bus id, scan thru all available if not set [Default: 1]\n"
"\t-i2caddr, I2C ADC device adress, found via 'i2cdetect' [Default: 0x48]\n"
"\t-test, Enable test mode if set and report min/max ADC value\n"
"\t-adcmin, ADC minimal value [Default: 0]\n"
"\t-adcmax, ADC maximal value [Default: 4096]\n"
"\t-adcreverse, reverse ADC value if set\n"
"\t-setPCA9633path, Full path to setPCA9633 folder [Default: /home/pi/Freeplay/setPCA9633/]\n"
"\t-updaterate, Time between each update, in msec [Default: 250]\n"
"\t-debug, optional, 1=full(will spam logs), 0 if not set\n"
,programversion);
}

int main(int argc, char *argv[]){ //main
	strcpy(setPCA9633_path,"/home/pi/Freeplay/setPCA9633/"); //init
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-debug")==0){debug=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-i2cbus")==0){i2c_bus=atoi(argv[i+1]);if(strstr(argv[i+1],"-")){i2c_bus=-i2c_bus;}
		}else if(strcmp(argv[i],"-i2caddr")==0){sscanf(argv[i+1], "%x", &i2c_addr);
		}else if(strcmp(argv[i],"-test")==0){test_mode=true;
		}else if(strcmp(argv[i],"-adcreverse")==0){adc_reverse=true;
		}else if(strcmp(argv[i],"-adcmin")==0){adc_low_value=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-adcmax")==0){adc_high_value=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-setPCA9633path")==0){strcpy(setPCA9633_path,argv[i+1]);
		}else if(strcmp(argv[i],"-updaterate")==0){update_interval=atoi(argv[i+1]);}
	}
	
	if(access(setPCA9633_path,F_OK)!=0){printf("Failed to access '%s'\n",setPCA9633_path); return 1;}
	
	for(int i=(i2c_bus<0)?0:i2c_bus;i<10;i++){ //detect i2c bus
		sprintf(i2c_path,"/dev/i2c-%d",i); //generate i2c bus full path
		if((i2c_handle=open(i2c_path,O_RDWR))>=0){ //open i2c handle
			if(ioctl(i2c_handle,I2C_SLAVE,i2c_addr)>=0){ //i2c adress detected
				if(read(i2c_handle,i2c_buffer,2)==2){ //success read
					i2c_bus=i; i2c_addr_valid=true;
					break; //exit loop
				}
			}
			close(i2c_handle); //close i2c handle
		}
	}
	
	if(i2c_addr_valid){debug_print("Bus %d : 0x%02x detected\n",i2c_bus,i2c_addr); //bus detected
	}else{debug_print("Failed, 0x%02x not detected on any bus, Exiting\n",i2c_addr);return(1);} //failed
	
	if(i2c_addr>=0x48&&i2c_addr<=0x4F){MCP3021A_detected=true; debug_print("MCP3021A detected\n");
	}else{debug_print("Failed, I2C address out of range, Exiting\n"); return(1);} //failed
	
	
	while(true){
		chdir(setPCA9633_path); //change default dir
		i2c_retry=0; //reset retry counter
		
		while(i2c_retry<3){
			adc_raw_value=-1;
			if((i2c_handle=open(i2c_path,O_RDWR))<0){
				debug_print("Failed to open the I2C bus : %s, retry in %dmsec\n",i2c_path,update_interval);
				i2c_retry=3; //no need to retry since failed to open I2C bus itself
			}else{
				if(ioctl(i2c_handle,I2C_SLAVE,i2c_addr)<0){ //access i2c device, allow retry if failed
					debug_print("Failed to access I2C device : %02x, retry in 1sec\n",i2c_addr);
				}else{ //success
					if(MCP3021A_detected){
						if(read(i2c_handle,i2c_buffer,2)!=2){debug_print("Failed to read data from I2C device : %04x, retry in 1sec\n",i2c_addr);
						}else{ //success
							adc_raw_value=(i2c_buffer[0]<<8)|(i2c_buffer[1]&0xff); //combine buffer bytes into integer
						}
					}
					
					if(adc_raw_value<0){debug_print("Warning, ADC return wrong data : %d\n",adc_raw_value);
					}else{ //success
						if(test_mode){
							if(adc_debug_low_value==-1){adc_debug_low_value=adc_raw_value;} //update low value
							if(adc_debug_high_value==-1){adc_debug_high_value=adc_raw_value;} //update high value
							if(adc_raw_value<adc_debug_low_value){adc_debug_low_value=adc_raw_value;} //update min debug value
							if(adc_raw_value>adc_debug_high_value){adc_debug_high_value=adc_raw_value;} //update max debug value
							debug_print("ADC : min:%d , max:%d\n",adc_debug_low_value,adc_debug_high_value);
						}
						
						if((abs(adc_raw_value-adc_last_value)>50||adc_last_value==-1)){
							adc_last_value=adc_raw_value; //backup value
							brightness_value=nns_map_int(adc_last_value,adc_low_value,adc_high_value,0,255); //compute backlight value
							if(adc_reverse){brightness_value=255-brightness_value;} //reverse value if needed
							debug_print("ADC value : %d, Brightness value : %d\n",adc_raw_value,brightness_value);
							
							if(!test_mode){ //fpbrightness.val update in not in test mode
								temp_filehandle=fopen("fpbrightness.val","wb"); //open file handle
								fprintf(temp_filehandle,"%d",brightness_value); //write data
								fclose(temp_filehandle); //close file handle
							}
							
							chdir(setPCA9633_path); //change directory
							sprintf(setPCA9633_command, "./setPCA9633 --verbosity=0 --i2cbus=1 --address=0x62 --mode1=0x01 --mode2=0x15 --led0=PWM --pwm0=0x%02x > /dev/null", brightness_value); //update command
							system(setPCA9633_command); //run command
						}
					}
				}
				close(i2c_handle);
			}
			
			if(adc_raw_value<0){
				i2c_retry++; //something failed at one point so retry
				if(i2c_retry>2){debug_print("Warning, ADC failed 3 times, skipping until next update\n");}else{sleep(1);}
			}else{i2c_retry=3;} //data read with success, no retry
		}
		
		usleep(update_interval*1000);
	}
	
	return(0);
}