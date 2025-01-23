/*
 * Copyright (c) Krzesimir Hyżyk - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Created on 19/11/2024 by Krzesimir Hyżyk
 */



#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>



#define ASSERT(x) if (!(x)){printf("%u(%s): %s: Assertion failed\n",__LINE__,__func__,#x);_Exit(1);}



#define WING_CHORD 8
#define WING_THICKNESS 0.75
#define WING_CHAMBER 0.275
#define WING_CHAMBER_OFFSET 2.4

#define AIRFOIL_POINT_COUNT 100



static const char _xfoil_input[]="LOAD /tmp/xfoil_airfoil.dat\n\nMDES\nFILT\nEXEC\n\nPANE\nOPER\nITER 70\nRE 50000\nVISC 50000\nPACC\n/tmp/xfoil_output.txt\n\nALFA 0\nPACC\nVISC\n\nQUIT\n";



static pid_t _x_server_pid;



static void _hide_output(void){
	int fd=open("/dev/null",O_WRONLY);
	ASSERT(fd>=0);
	dup2(fd,1);
	dup2(fd,2);
	close(fd);
}



static void _create_empty_x_server(void){
	pid_t child=fork();
	if (!child){
		_hide_output();
		char*const argv[]={
			"/usr/bin/Xvfb",
			":1",
			NULL
		};
		execve("/usr/bin/Xvfb",argv,NULL);
		_Exit(1);
	}
	ASSERT(child>=0);
	_x_server_pid=child;
	while (1){
		child=fork();
		if (!child){
			_hide_output();
			char*const argv[]={
				"/usr/bin/xset",
				"-display",":1",
				"q",
				NULL
			};
			execve("/usr/bin/xset",argv,NULL);
			_Exit(1);
		}
		ASSERT(child>=0);
		int status=-1;
		if (waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status)){
			break;
		}
		usleep(10000);
	}
}



static void _stop_empty_x_server(void){
	kill(_x_server_pid,SIGTERM);
	ASSERT(waitpid(_x_server_pid,NULL,0)==_x_server_pid);
}



static void _execute_xfoil(void){
	unlink("/tmp/xfoil_output.txt");
	int input_pipe[2];
	ASSERT(!pipe(input_pipe));
	pid_t child=fork();
	if (!child){
		close(input_pipe[1]);
		dup2(input_pipe[0],STDIN_FILENO);
		_hide_output();
		char*const argv[]={
			"/usr/bin/xfoil",
			NULL
		};
		char*const envp[]={
			"DISPLAY=:1",
			NULL
		};
		execve("/usr/bin/xfoil",argv,envp);
		_Exit(1);
	}
	ASSERT(child>=0);
	close(input_pipe[0]);
	ASSERT(write(input_pipe[1],_xfoil_input,sizeof(_xfoil_input))==sizeof(_xfoil_input));
	close(input_pipe[1]);
	ASSERT(waitpid(child,NULL,0)==child);
	unlink(":00.bl");
}



static void _generate_airfoil(void){
	double m=WING_CHAMBER/WING_CHORD;
	double p=WING_CHAMBER_OFFSET/WING_CHORD;
	double t=WING_THICKNESS/WING_CHORD;
	printf("NACA %u%u%02u airfoil\n",(unsigned int)round(m*100),(unsigned int)round(p*10),(unsigned int)round(t*100));
	double points[(AIRFOIL_POINT_COUNT<<1)-1][2];
	for (unsigned int i=0;i<AIRFOIL_POINT_COUNT;i++){
		double x=0.5-cos(3.141592653589793*i/(AIRFOIL_POINT_COUNT-1))/2;
		double x2=x*x;
		double d=m/(x<p?p*p:(1-p)*(1-p));
		double y=d*(x<p?2*p*x-x*x:1-2*p+2*p*x-x*x);
		double a=2*d*(p-x);
		double u=5*t*(0.2969*sqrt(x)-0.126*x-0.3516*x2+0.2843*x*x2-0.1015*x2*x2)/sqrt(1+a*a);
		points[AIRFOIL_POINT_COUNT-1-i][0]=x+a*u;
		points[AIRFOIL_POINT_COUNT-1-i][1]=y-u;
		points[AIRFOIL_POINT_COUNT-1+i][0]=x-a*u;
		points[AIRFOIL_POINT_COUNT-1+i][1]=y+u;
	}
	int fd=open("/tmp/xfoil_airfoil.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	ASSERT(fd>=0);
	char line_buffer[256];
	for (unsigned int i=0;i<(AIRFOIL_POINT_COUNT<<1)-1;i++){
		unsigned int line_length=snprintf(line_buffer,sizeof(line_buffer),"%.18lf %.18lf\n",points[i][0],points[i][1]);
		ASSERT(write(fd,line_buffer,line_length)==line_length);
	}
	close(fd);
}



static void _parse_xfoil_output(void){
	int fd=open("/tmp/xfoil_output.txt",O_RDONLY);
	struct stat stat_buf;
	ASSERT(fd>=0&&!fstat(fd,&stat_buf));
	const char* address=mmap(NULL,stat_buf.st_size,PROT_READ,MAP_PRIVATE,fd,0);
	ASSERT(address!=MAP_FAILED);
	close(fd);
	unsigned int i=0;
	for (unsigned int j=0;i<stat_buf.st_size&&j<12;j+=(address[i]=='\n')){
		i++;
	}
	for (;i<stat_buf.st_size&&(address[i]==' '||address[i]=='\n');i++);
	for (;i<stat_buf.st_size&&address[i]!=' ';i++);
	double coeffs[2];
	ASSERT(sscanf(address+i,"%lf %lf",coeffs,coeffs+1)==2);
	munmap((void*)address,stat_buf.st_size);
	printf("Cl=%.4lf eta=%.4lf\n",coeffs[0],coeffs[1]/coeffs[0]);
}



int main(void){
	_generate_airfoil();
	_create_empty_x_server();
	_execute_xfoil();
	_stop_empty_x_server();
	_parse_xfoil_output();
	return 0;
}
