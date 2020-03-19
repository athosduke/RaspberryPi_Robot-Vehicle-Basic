#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include "import_registers.h"
#include "cm.h"
#include "gpio.h"
#include "spi.h"
#include "pwm.h"
#include "io_peripherals.h"
#include "enable_pwm_clock.h"

#define PWM_RANGE 100

struct pause_flag
{
  pthread_mutex_t lock;
  bool            pause;
};

struct done_flag
{
  pthread_mutex_t lock;
  bool            done;
};

struct thread_parameter
{
  volatile struct gpio_register * gpio;
  volatile struct pwm_register  * pwm;
  int                             pin;
  struct pause_flag *             pause;
  struct done_flag *              done;
};

struct key_thread_parameter
{
  struct done_flag *  done;
  struct pause_flag * pause1;
  struct pause_flag * pause2;
  struct pause_flag * pause3;
  struct pause_flag * pause4;
  struct pause_flag * pause5;
  struct pause_flag * pause6;
};

int  Tstep = 50;  /* PWM time resolution, number used for usleep(Tstep) */
int  Tlevel = 5;  /* repetition count of each light level, eg. repeat 12% light level for 5 times. */
int inc = 0;
int dec = 0;
int forw = 0; //forward = 1
int back = 0; //backward  = 1
int sb = 0; // shortbreak =1
int left = 0;
int right = 0;
int init = 1;
double vm=110;
double L=14;
int quit = 0;


void DimLevUnit(int Level, int pin, volatile struct gpio_register *gpio)
{
  int ONcount, OFFcount;

  ONcount = Level;
  OFFcount = 100 - Level;

  /* create the output pin signal duty cycle, same as Level */
  GPIO_SET( gpio, pin ); /* ON LED at GPIO 18 */
  while (ONcount > 0)
  {
    usleep( Tstep );
    ONcount = ONcount - 1;
  }
  GPIO_CLR( gpio, pin ); /* OFF LED at GPIO 18 */
  while (OFFcount > 0)
  {
    usleep( Tstep );
    OFFcount = OFFcount - 1;
  }
}

void *ThreadSW( void * arg )
{
  struct thread_parameter *parameter = (struct thread_parameter *) arg;
  pthread_mutex_lock(&(parameter->done->lock));
  while(!(parameter->done->done)){
  	pthread_mutex_unlock(&(parameter->done->lock));
  	//forward 10
	if (forw && !back && !sb){
		if (parameter->pin == 18){
			GPIO_SET( (parameter->gpio), 18);
		}
		if (parameter->pin == 19){
			GPIO_CLR( (parameter->gpio), 19);
		}
		if (parameter->pin == 22){
			GPIO_SET( (parameter->gpio), 22);
		}
		if (parameter->pin == 23){
			GPIO_CLR( (parameter->gpio), 23);
		}
	}
  	//backward 01 
	else if (!forw && back && !sb){
		if (parameter->pin == 18){
			GPIO_CLR( (parameter->gpio), 18);
		}
		if (parameter->pin == 19){
			GPIO_SET( (parameter->gpio), 19);
		}
		if (parameter->pin == 22){
			GPIO_CLR( (parameter->gpio), 22);
		}
		if (parameter->pin == 23){
			GPIO_SET( (parameter->gpio), 23);
		}
	}
  	//stop 00
	else if (!forw && !forw && !sb){
		if (parameter->pin == 18){
			GPIO_CLR( (parameter->gpio), 18);
		}
		if (parameter->pin == 19){
			GPIO_CLR( (parameter->gpio), 19);
		}
		if (parameter->pin == 22){
			GPIO_CLR( (parameter->gpio), 22);
		}
		if (parameter->pin == 23){
			GPIO_CLR( (parameter->gpio), 23);
		}
	}
  	//short break 11
  	else if (sb){
  		if (parameter->pin == 18){
			GPIO_SET( (parameter->gpio), 18);
		}
		if (parameter->pin == 19){
			GPIO_SET( (parameter->gpio), 19);
		}
		if (parameter->pin == 22){
			GPIO_SET( (parameter->gpio), 22);
		}
		if (parameter->pin == 23){
			GPIO_SET( (parameter->gpio), 23);
		}
	}
	pthread_mutex_lock(&(parameter->done->lock));
  }
  pthread_mutex_unlock(&(parameter->done->lock));

  return 0;
}

void *ThreadHW( void * arg )
{
  int                       iterations; /* used to limit the number of dimming iterations */
  int                       Timeu;      /* dimming repetition of each level */
  int                       DLevel;     /* dimming level as duty cycle, 0 to 100 percent */
  struct thread_parameter * parameter = (struct thread_parameter *)arg;
  int                       speed = 100;
  int turnspeed = (int)((vm - L * 3.1415926 / 12 / 1)/vm*100);

  pthread_mutex_lock( &(parameter->done->lock) );
  while (!(parameter->done->done))
  {
    pthread_mutex_unlock( &(parameter->done->lock) );

    pthread_mutex_lock( &(parameter->pause->lock) );
    while (parameter->pause->pause)
    {
      pthread_mutex_unlock( &(parameter->pause->lock) );
      usleep( 10000 ); /* 10ms */
      pthread_mutex_lock( &(parameter->pause->lock) );
    }
    pthread_mutex_unlock( &(parameter->pause->lock) );

    //constant speed
    while(!inc && !dec){
    	//right
    	while(right>0){
    		if (parameter->pin == 12){
    			parameter->pwm->DAT1 = 100;
    		}
    		else if(parameter->pin == 13){
    			parameter->pwm->DAT2 = turnspeed;
    		}
    	}

	    //left
	    while(left>0){
	    	if (parameter->pin == 12){
	    		parameter->pwm->DAT1 = turnspeed;
	    	}
	    	else if(parameter->pin == 13){
	    		parameter->pwm->DAT2 = 100;
	    	}
        }
        //if initail condition: full speed
	    if (init == 1){
	    	speed = 100;
	    }

	    //constant speed
	    if (parameter->pin == 12){
        parameter->pwm->DAT1 = speed;
      	}
	    else if (parameter->pin == 13){
	    	parameter->pwm->DAT2 = speed;
      	}
	    usleep(10000); //10ms
	    if(inc || (!forw && !back) || dec){
	    	break;
      	}
    }
    //change speed
    //inc
    if(inc == 1){
    	int subspeed = speed;
    	while((subspeed<PWM_RANGE) && (subspeed < speed+10)){
    		if(parameter->pin == 12){
    			parameter->pwm->DAT1 = subspeed;
        	}
    		else if(parameter->pin == 13){
    			parameter->pwm->DAT2 = subspeed;
        	}
    		usleep(Tlevel*Tstep*100);
    		subspeed += 1;
    	}
    	speed = subspeed;
      	//printf("increase speed to %i\n",speed);
    	inc = 0;
    }

    //dec
    if(dec == 1){
    	int subspeed = speed;
    	while((subspeed>40) && (subspeed > speed-10)){
    		if(parameter->pin == 12){
    			parameter->pwm->DAT1 = subspeed;
        	}
    		else if(parameter->pin == 13){
    			parameter->pwm->DAT2 = subspeed;
        	}
    		usleep(Tlevel*Tstep*100);
    		subspeed -= 1;
    	}
    	speed = subspeed;
    	dec = 0;
      //printf("decrease speed to %i\n",speed);
      
    }
    pthread_mutex_lock( &(parameter->done->lock) );
  }
  pthread_mutex_unlock( &(parameter->done->lock) );

  return 0;
}

int get_pressed_key(void)
{
  struct termios  original_attributes;
  struct termios  modified_attributes;
  int             ch;

  tcgetattr( STDIN_FILENO, &original_attributes );
  modified_attributes = original_attributes;
  modified_attributes.c_lflag &= ~(ICANON | ECHO);
  modified_attributes.c_cc[VMIN] = 1;
  modified_attributes.c_cc[VTIME] = 0;
  tcsetattr( STDIN_FILENO, TCSANOW, &modified_attributes );

  ch = getchar();

  tcsetattr( STDIN_FILENO, TCSANOW, &original_attributes );

  return ch;
}

void *ThreadKey( void * arg )
{
  struct key_thread_parameter *thread_key_parameter = (struct key_thread_parameter *)arg;
  bool done;

  do
  {
    switch (get_pressed_key())
    {
      case 'q':
        done = true;
        quit = 1;
        /* unpause everything */
        pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        thread_key_parameter->pause1->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        thread_key_parameter->pause2->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
        thread_key_parameter->pause3->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
        thread_key_parameter->pause4->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
        thread_key_parameter->pause5->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
        thread_key_parameter->pause6->pause = false;
        pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );

        /* indicate that it is time to shut down */
        pthread_mutex_lock( &(thread_key_parameter->done->lock) );
        thread_key_parameter->done->done = true;
        pthread_mutex_unlock( &(thread_key_parameter->done->lock) );
        break;

      //pause
      case 's':
        pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        thread_key_parameter->pause1->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        thread_key_parameter->pause2->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
        thread_key_parameter->pause3->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
        thread_key_parameter->pause4->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
        thread_key_parameter->pause5->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
        pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
        thread_key_parameter->pause6->pause = true;
        pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );
        forw = 0;
        back = 0;
        sb = 0;
        inc = 0;
        dec = 0;
        left = 0;
        right = 0;
        printf("stop\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
        break;

      //forward
      case 'f':
        if (!forw && !back){
          pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
          thread_key_parameter->pause2->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
          thread_key_parameter->pause3->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
          thread_key_parameter->pause4->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
          thread_key_parameter->pause5->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
          thread_key_parameter->pause6->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );
        }
      	//previous not go forward
      	if(forw!=1){
      		init = 1;
      	}
      	inc = 0;
      	dec = 0;
      	left = 0;
      	right = 0;
      	//if going backward
      	if (back==1){
      		pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = true;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = true;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 1;
        	usleep(500000);
        	pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        	thread_key_parameter->pause1->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 0;
      	}
      	forw = 1;
      	back = 0;
      	printf("forward\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //forward
      case 'w':
        if (!forw && !back){
          pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
          thread_key_parameter->pause2->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
          thread_key_parameter->pause3->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
          thread_key_parameter->pause4->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
          thread_key_parameter->pause5->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
          thread_key_parameter->pause6->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );
        }
      	//previous not go forward
      	if(forw!=1){
      		init = 1;
      	}
      	inc = 0;
      	dec = 0;
      	left = 0;
      	right = 0;
      	//if going backward
      	if (back==1){
      		pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = true;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = true;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 1;
        	usleep(500000);
        	pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        	thread_key_parameter->pause1->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 0;
      	}
      	forw = 1;
      	back = 0;
      	printf("forward\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //backward
      case 'b':
        if (!forw && !back){
          pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
          thread_key_parameter->pause2->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
          thread_key_parameter->pause3->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
          thread_key_parameter->pause4->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
          thread_key_parameter->pause5->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
          thread_key_parameter->pause6->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );
        }
        if(back!=1){
        	init = 1;
        }
      	inc = 0;
      	dec = 0;
      	right = 0;
      	left = 0;
      	//if going forward
      	if (forw==1){
      		pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          	thread_key_parameter->pause1->pause = true;
          	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = true;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 1;
        	usleep(500000);
        	pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        	thread_key_parameter->pause1->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 0;
      	}
      	forw = 0;
      	back = 1;
      	printf("backward\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;


      //backward
      case 'x':
        if (!forw && !back){
          pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          thread_key_parameter->pause1->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
          thread_key_parameter->pause2->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause3->lock) );
          thread_key_parameter->pause3->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause3->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause4->lock) );
          thread_key_parameter->pause4->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause4->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause5->lock) );
          thread_key_parameter->pause5->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause5->lock) );
          pthread_mutex_lock( &(thread_key_parameter->pause6->lock) );
          thread_key_parameter->pause6->pause = false;
          pthread_mutex_unlock( &(thread_key_parameter->pause6->lock) );
        }
        if(back!=1){
        	init = 1;
        }
      	inc = 0;
      	dec = 0;
      	right = 0;
      	left = 0;
      	//if going forward
      	if (forw==1){
      		pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
          	thread_key_parameter->pause1->pause = true;
          	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
          	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = true;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 1;
        	usleep(500000);
        	pthread_mutex_lock( &(thread_key_parameter->pause1->lock) );
        	thread_key_parameter->pause1->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause1->lock) );
        	pthread_mutex_lock( &(thread_key_parameter->pause2->lock) );
        	thread_key_parameter->pause2->pause = false;
        	pthread_mutex_unlock( &(thread_key_parameter->pause2->lock) );
        	sb = 0;
      	}
      	forw = 0;
      	back = 1;
      	printf("backward\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;


      //faseter
      case 'i':
      	init = 0;
      	inc = 1;
      	dec = 0;
      	printf("faster\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //slower
      case 'j':
      	init = 0;
      	inc = 0;
      	dec = 1;
      	printf("slower\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //left	
      case 'l':
      	inc = 0;
      	dec = 0;
      	left++;
      	printf("left\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //left	
      case 'a':
      	inc = 0;
      	dec = 0;
      	left++;
      	printf("left\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //right	
      case 'r':
      	inc = 0;
      	dec = 0;
      	right++;
      	printf("right\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      //right	
      case 'd':
      	inc = 0;
      	dec = 0;
      	right++;
      	printf("right\n");//:init:%i,forw:%i,back:%i,sb:%i,inc:%i,dec:%i,left:%i,right:%i\n",init,forw,back,sb,inc,dec,left,right);
      	break;

      default:
        break;
    }
  } while (!done);
  printf( "key thread exiting\n" );

  return (void *)0;
}

void *turntime(void *t){
	while (!quit){
		if(left||right){
			usleep(100000);
		if(left){
			left--;
		}
		if(right){
			right--;
		}
		}
	}
}

int main( void )
{
  volatile struct io_peripherals *io;
  pthread_t                       thread12_handle;
  pthread_t                       thread13_handle;
  pthread_t                       thread18_handle;
  pthread_t                       thread19_handle;
  pthread_t                       thread22_handle;
  pthread_t                       thread23_handle;
  pthread_t                       thread_key_handle;
  pthread_t                       turntiming;
  struct done_flag                done   = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause1 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause2 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause3 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause4 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause5 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct pause_flag               pause6 = {PTHREAD_MUTEX_INITIALIZER, false};
  struct thread_parameter         thread12_parameter;
  struct thread_parameter         thread13_parameter;
  struct thread_parameter         thread18_parameter;
  struct thread_parameter         thread19_parameter;
  struct thread_parameter         thread22_parameter;
  struct thread_parameter         thread23_parameter;
  struct key_thread_parameter     thread_key_parameter;

  io = import_registers();
  if (io != NULL)
  {
    /* print where the I/O memory was actually mapped to */
    printf( "mem at 0x%8.8X\n", (unsigned long)io );
    printf( "press f/w to move forward at full speed \n");
    printf("press b/x to move backward at full speed\n");
    printf("duplicate forward/backward would be ignored\n");
    printf("press s to stop/pause\n");
    printf("press i/j to increase/decrease the speed by 10%\n");
    printf("press r/a to turn right 15 degree\n");
    printf("press l/d to turn left 15 degree\n");
    printf("press q to quit the program\n");

    enable_pwm_clock( io );

    /* set the pin function to alternate function 0 for GPIO12 */
    /* set the pin function to alternate function 0 for GPIO13 */
    io->gpio.GPFSEL1.field.FSEL2 = GPFSEL_ALTERNATE_FUNCTION0;
    io->gpio.GPFSEL1.field.FSEL3 = GPFSEL_ALTERNATE_FUNCTION0;
    io->gpio.GPFSEL1.field.FSEL8 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL1.field.FSEL9 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL2.field.FSEL2 = GPFSEL_OUTPUT;
    io->gpio.GPFSEL2.field.FSEL3 = GPFSEL_OUTPUT;

    /* configure the PWM channels */
    io->pwm.RNG1 = PWM_RANGE;     /* the default value */
    io->pwm.RNG2 = PWM_RANGE;     /* the default value */
    io->pwm.CTL.field.MODE1 = 0;  /* PWM mode */
    io->pwm.CTL.field.MODE2 = 0;  /* PWM mode */
    io->pwm.CTL.field.RPTL1 = 1;  /* not using FIFO, but repeat the last byte anyway */
    io->pwm.CTL.field.RPTL2 = 1;  /* not using FIFO, but repeat the last byte anyway */
    io->pwm.CTL.field.SBIT1 = 0;  /* idle low */
    io->pwm.CTL.field.SBIT2 = 0;  /* idle low */
    io->pwm.CTL.field.POLA1 = 0;  /* non-inverted polarity */
    io->pwm.CTL.field.POLA2 = 0;  /* non-inverted polarity */
    io->pwm.CTL.field.USEF1 = 0;  /* do not use FIFO */
    io->pwm.CTL.field.USEF2 = 0;  /* do not use FIFO */
    io->pwm.CTL.field.MSEN1 = 1;  /* use M/S algorithm */
    io->pwm.CTL.field.MSEN2 = 1;  /* use M/S algorithm */
    io->pwm.CTL.field.CLRF1 = 1;  /* clear the FIFO, even though it is not used */
    io->pwm.CTL.field.PWEN1 = 1;  /* enable the PWM channel */
    io->pwm.CTL.field.PWEN2 = 1;  /* enable the PWM channel */

    thread12_parameter.pin = 12;
    thread12_parameter.gpio = &(io->gpio);
    thread12_parameter.pwm = &(io->pwm);
    thread12_parameter.done = &done;
    thread12_parameter.pause = &pause1;
    thread13_parameter.pin = 13;
    thread13_parameter.pwm = &(io->pwm);
    thread13_parameter.gpio = &(io->gpio);
    thread13_parameter.done = &done;
    thread13_parameter.pause = &pause2;
    thread18_parameter.pin = 18;
    thread18_parameter.pwm = &(io->pwm);
    thread18_parameter.gpio = &(io->gpio);
    thread18_parameter.done = &done;
    thread18_parameter.pause = &pause3;
    thread19_parameter.pin = 19;
    thread19_parameter.pwm = &(io->pwm);
    thread19_parameter.gpio = &(io->gpio);
    thread19_parameter.done = &done;
    thread19_parameter.pause = &pause4;
    thread22_parameter.pin = 22;
    thread22_parameter.gpio = &(io->gpio);
    thread22_parameter.pwm = &(io->pwm);
    thread22_parameter.done = &done;
    thread22_parameter.pause = &pause5;
    thread23_parameter.pin = 23;
    thread23_parameter.pwm = &(io->pwm);
    thread23_parameter.gpio = &(io->gpio);
    thread23_parameter.done = &done;
    thread23_parameter.pause = &pause6;

    thread_key_parameter.done = &done;
    thread_key_parameter.pause1 = &pause1;
    thread_key_parameter.pause2 = &pause2;
    thread_key_parameter.pause3 = &pause3;
    thread_key_parameter.pause4 = &pause4;
    thread_key_parameter.pause5 = &pause5;
    thread_key_parameter.pause6 = &pause6;

    pthread_create( &turntiming, 0, turntime, NULL);
    pthread_create( &thread12_handle, 0, ThreadHW, (void *)&thread12_parameter );
    pthread_create( &thread13_handle, 0, ThreadHW, (void *)&thread13_parameter );
    pthread_create( &thread18_handle, 0, ThreadSW, (void *)&thread18_parameter );
    pthread_create( &thread19_handle, 0, ThreadSW, (void *)&thread19_parameter );
    pthread_create( &thread22_handle, 0, ThreadSW, (void *)&thread22_parameter );
    pthread_create( &thread23_handle, 0, ThreadSW, (void *)&thread23_parameter );
    pthread_create( &thread_key_handle, 0, ThreadKey, (void *)&thread_key_parameter );

    pthread_join( turntiming, 0);
    pthread_join( thread12_handle, 0 );
    pthread_join( thread13_handle, 0 );
    pthread_join( thread18_handle, 0 );
    pthread_join( thread19_handle, 0 );
    pthread_join( thread22_handle, 0 );
	pthread_join( thread23_handle, 0 );
    pthread_join( thread_key_handle, 0 );
  }
  printf("quiting...");
  return 0;
}

