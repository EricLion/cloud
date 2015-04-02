#include <stdio.h>


#define QUEUE_SIZE 1024
#define MAX_BUFF_SIZE 1024

typedef struct
{
	char buf[MAX_BUFF_SIZE];
}ElementType;


struct kfifo
{
	ElementType data[QUEUE_SIZE];
	unsigned int in;
	unsigned int out;
};



int kfifo_put( struct kfifo *fifo, ElementType element )
{

	if ( QUEUE_SIZE - fifo->in + fifo->out == 0 )
		 return 0;
		 
		 unsigned int index = fifo->in & (QUEUE_SIZE - 1);
		 
		 printf("put index = %d\n",index);
		 
		 __sync_synchronize();      //确保取出的out是最新的（它是这么说的，但极度怀疑不需要）
		 
		 fifo->data[index] = element;
		 
		 __sync_synchronize();      //确保先写入数据再更新in
		 
		 fifo->in++;
		 
		 return 1;
}


int kfifo_get( struct kfifo *fifo, ElementType *element )
{
	if ( fifo->in - fifo->out == 0 )
		return 0;
		
		unsigned int index = fifo->out & (QUEUE_SIZE - 1);
		
		printf("out index = %d\n",index);
		
		__sync_synchronize();       //确保读出的in是最新的（同上）
		
		*element = fifo->data[index];
		
		__sync_synchronize();       //确保先读取数据再更新out
		
		fifo->out++;
		
		return 1;
}



void main()
{
	int ret = 0,i;
	struct kfifo quene;
	ElementType et;	
	quene.in = 0;
	quene.out = 0;

	for(i = 0;i < QUEUE_SIZE; i++)
	{
		 et.buf[0] = i;
		 et.buf[1] = '\0';
			
		 ret = kfifo_put(&quene,et);
		 if(!ret)
		 {
			 printf("put error !\n");
		 }
	}
	printf("put count = %d\n",quene.in);


	for(i = 0;i < QUEUE_SIZE; i++)
	{
		ret = kfifo_get(&quene,&et);
		
		if(!ret)
		{
	       printf("get error !\n");
		}
		
		printf("get value = %s\n",et.buf);
	}


	printf("get count = %d\n",quene.out);
	
}
