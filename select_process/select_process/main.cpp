#define _CRT_SECURE_NO_WARNINGS

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <vector>

#define MEMORY 10000
#define SELECT_PRO_NUM 4
#define THRESHOLD_COUNT 5
using namespace std;

//定义进程属性
struct processes
{
	int memory;			//当前进程需求memory
	int waitcount;		//当前进程等待次数
	int location;		//当前进程执行到的位置
	int process_ID;		//进程ID	0代表IO类型， 1代表非IO类型
	int process_type;	//进程类型
	processes(){};

	
};
typedef struct processes processes;



//打开文件并读入数据
int read_data(const char *filename, int source_data[8][200]);

//从IO类型和非IO类型进程中分别拷贝IOcount和NIOcount个数据到队列中
void init_data(int IO_Data[8][200], int NIO_Data[8][200], int IOcount, int NIOcount, struct processes *pro);

//挑选该条件下的进程组合
void pick_out_process(int IO_Data[8][200], int NIO_Data[8][200], processes *Pro, int pro_num, const char *out_filename);

//选出本次要执行的进程，返回目前系统中剩余进程个数
int select_process(int IO_Data[8][200], int NIO_Data[8][200], processes *pro, int pro_num, const char *out_filename);

//从排序队列中里删除memory为 -1 的进程（因为该进程已经不存在了）
int del_zero_memory(processes *pro, int pro_num);


//排序比较
int mycompare(const void*a, const void *b)
{
	processes *t1, *t2;
	t1 = (processes *)a; 
	t2 = (processes *)b;
	if(t1->memory <= t2->memory)
		return -1;
	else
		return 1;
	
}

vector<processes> wait_process;



const int io_nio[12][2] ={{1,4}, {1, 8}, {2, 4}, {2, 8}, {4, 1}, {4, 2}, {4, 4}, {4, 8}, {8, 1}, {8, 2}, {8, 4}, {8, 8}}; 
const char *IO_name = "io-64g-online.txt";
const char *NIO_name = "non-io-online.txt";
int IO_Data[8][200], NIO_Data[8][200];

int main(int argc, char *argv[])
{
	
	struct processes *Pro;
	char buf[20];
	char temp[3];
	int IOcount;		//io进程个数
	int NIOcount;		//非io进程个数
	memset(IO_Data, -1, 8*200*sizeof(int));
	memset(NIO_Data, -1, 8*200*sizeof(int));

	FILE *fp;
	//读入数据
	read_data(IO_name, IO_Data);
	read_data(NIO_name, NIO_Data);

	//
	for(int i = 0; i < 12; i++){
		IOcount = io_nio[i][0];
		NIOcount = io_nio[i][1];
		Pro = (struct processes *) malloc((IOcount + NIOcount) * sizeof(struct processes));
		init_data(IO_Data, NIO_Data, IOcount, NIOcount, Pro);
		
		memset(buf, 0, 20 * sizeof(char));
		strcat(buf, "out");
		_itoa(i, temp, 10);
		strcat(buf, temp);
		strcat(buf, ".txt");
		fp = fopen(buf, "w");
		fclose(fp);

		//挑选该条件下的进程组合
		pick_out_process(IO_Data, NIO_Data, Pro, IOcount + NIOcount, buf);
		free(Pro);
	}
	return 0;
}

//挑选该条件下的进程组合
void pick_out_process(int IO_Data[8][200], int NIO_Data[8][200], processes *Pro, int pro_num, const char *out_filename)
{
	int left_pro_num = pro_num;
	bool need_end = true;
	qsort(Pro, left_pro_num, sizeof(struct processes), mycompare);
	while (true)
	{
		need_end = true;
		for(int i = 0; i < left_pro_num; i++){
			if(Pro[i].memory != -1)
				need_end = false;
		}
		if(need_end)
			return;
		left_pro_num = select_process(IO_Data, NIO_Data, Pro, left_pro_num, out_filename);
		qsort(Pro, left_pro_num, sizeof(struct processes), mycompare);
	}
	
}

//初始化数据, 从IO类型和非IO类型进程中分别拷贝IOcount和NIOcount个数据到队列中
void init_data(int IO_Data[8][200], int NIO_Data[8][200], int IOcount, int NIOcount, processes *pro)
{
	for(int i = 0; i < IOcount; i++){
		pro[i].memory = IO_Data[i][0];
		pro[i].location = 0;
		pro[i].process_ID = i;
		pro[i].process_type = 0;
		pro[i].waitcount = 0;
	}

	for(int i = IOcount, j = 0; i < (IOcount + NIOcount); i++, j++){
		pro[i].memory = NIO_Data[j][0];
		pro[i].location = 0;
		pro[i].process_ID = j;
		pro[i].process_type = 1;
		pro[i].waitcount = 0;
	}
}

//打开文件并读入数据
int read_data(const char *filename, int source_data[8][200])
{
	int i=-1,j=0;
	int temp;
	char c;
	FILE* fp;
	
	fp=fopen(filename,"r");
	while(1)
	{
		fscanf(fp,"%d",&temp);
		source_data[j][++i]=temp;
		fscanf(fp,"%c",&c);

		if(feof(fp))
			break;
		
		if(c=='\n')
		{
			++j;
			i=-1;
		}
	}
	fclose(fp);

	return 0;
}

//选出本次要执行的进程, 返回目前系统中剩余进程个数
int select_process(int IO_Data[8][200], int NIO_Data[8][200], processes *pro, int pro_num, const char *out_filename)
{
	FILE* fp = NULL;
	int consume_memory = 0;			//目前已用的memory大小
	int left_memory = MEMORY;		//可用memory大小
	int need_memory = 0;			//排序中选出的n个进程需求的memory大小
	int wait_number = 0;			//wait队列中进程个数
	int left_pro_num = pro_num;
	//processes temp_pro;
	
	fp = fopen(out_filename, "a+");



	//1. 从wait队列中选取进程
	wait_number = (SELECT_PRO_NUM < wait_process.size()) ? SELECT_PRO_NUM : wait_process.size();

//	printf("wait_size = %d, wait_number = %d\t", wait_process.size(), wait_number);
	for(int i = 0; i < wait_number; ++i){
		consume_memory += wait_process[0].memory;
		fprintf(fp, "%d\t%d\t", wait_process[0].process_type, wait_process[0].process_ID);
		wait_process.erase(wait_process.begin());
	}

	//出去wait队列中进程的memory消耗后，剩余的memory
	left_memory = left_memory - consume_memory;

	//wait_process.insert(wait_process.end(), temp_pro);

	//2. 从排序队列中删除memory为 -1 的进程（因为该进程已经不存在了）， 并对排序队列补充新进程
	left_pro_num = del_zero_memory(pro, pro_num);

	//从排序队列中选取的进程
	int select_sort_num = SELECT_PRO_NUM - wait_number;
	for(int j = 0; j < left_pro_num - select_sort_num + 1; j++){
		for(int i = j; i < select_sort_num + j; i++){
			need_memory += pro[i].memory;
		}
		
		if(need_memory >= left_memory || j ==  left_pro_num - select_sort_num){
			for(int i = j; i < select_sort_num + j; i++){
				
				fprintf(fp, "%d\t%d\t", pro[i].process_type, pro[i].process_ID);

				//更新该位置的进程
				pro[i].location +=1;
				pro[i].waitcount = -1;
				if(pro[i].process_type == 0){
					pro[i].memory = IO_Data[pro[i].process_ID][pro[i].location];
				}else{
					pro[i].memory = NIO_Data[pro[i].process_ID][pro[i].location];
				}
			}
			break;
		}
	}
	//printf("select_sort_num = %d, left_pro_num = %d\n", select_sort_num, left_pro_num);
	//如果排序队列中剩余的进程数小于需要选取的进程数，则直接输出
	if( left_pro_num < select_sort_num){
		for(int i = 0; i < left_pro_num; i++){
				fprintf(fp, "%d\t%d\t", pro[i].process_type, pro[i].process_ID);
				//更新该位置的进程
				pro[i].location +=1;
				pro[i].waitcount = -1;
				if(pro[i].process_type == 0){
					pro[i].memory = IO_Data[pro[i].process_ID][pro[i].location];
				}else{
					pro[i].memory = NIO_Data[pro[i].process_ID][pro[i].location];
				}
			}
	}

	//3. 将排序队列中wait_count个数大于阈值的加入wait队列中，并对排序队列补充新进程
	for(int i = 0; i < left_pro_num; i++){
//		printf("pro_type = %d, pro_ID = %d, pro_waitcount = %d\n", pro[i].process_type, pro[i].process_ID, pro[i].waitcount);
		pro[i].waitcount++;
//		printf("wait_count = %d\t", pro[i].waitcount);
//		printf("pro_type = %d, pro_ID = %d, pro_waitcount = %d\n", pro[i].process_type, pro[i].process_ID, pro[i].waitcount);
		if(pro[i].waitcount >= THRESHOLD_COUNT){
			wait_process.insert(wait_process.end(), pro[i]);
			//更新该位置的进程
			pro[i].location +=1;
			pro[i].waitcount = 0;
			if(pro[i].process_type == 0){
				pro[i].memory = IO_Data[pro[i].process_ID][pro[i].location];
			}else{
				pro[i].memory = NIO_Data[pro[i].process_ID][pro[i].location];
			}
		}

	}

	fprintf(fp, "\n");
	fclose(fp);

	return left_pro_num;
}

//从排序队列中删除memory为 -1 的进程（因为该进程已经不存在了）
int del_zero_memory(processes *pro, int pro_num)
{
	int left_num = pro_num;
	for(int i = 0; i < left_num - 1; ++i){
		if(pro[i].memory == -1){
			memcpy(pro, (pro + 1), (left_num - 1) * sizeof(processes));
			--left_num;
			--i;
		}
	}
//	printf("left_number = %d\t", left_num);
	//处理最后一个process
	if(pro[left_num - 1].memory == -1)
		--left_num;

	return left_num;
}