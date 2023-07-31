build:
	g++ Consumer.cpp -o Consumer.out
	g++ Producer.cpp -o Producer.out
	touch shmfile semMutexFile semBufferFile semCountFile

clean:
	rm Consumer.out Producer.out shmfile semMutexFile semBufferFile semCountFile