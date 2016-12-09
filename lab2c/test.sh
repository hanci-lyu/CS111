# for k in m s; 
# do
# 	for j in 64 32 16 8 4 2 1; 
# 	do
# 		echo "./lab2c --t=8 --i=10000 --s=${k} --l=${j}"
# 		for i in `seq 10`; do ./lab2c --t=8 --i=10000 --s=${k} --l=${j}; done | grep 'corrected' | awk '{a+=$3} END{print a/NR}'
# 	done
# done

# for j in 8 4 2 1; 
# do
# 	echo "./lab2c --t=1 --i=10000 --l=${j}"
# 	for i in `seq 10`; do ./lab2c --t=1 --i=10000 --l=${j}; done | grep 'corrected' | awk '{a+=$3} END{print a/NR}'
# done

for sync in m s;
do
	for list in 64 32 16 8 4 2 1;
	do
		rm -f gmon.out
		./lab2c --t=8 --i=10000 --l=${list} --s=${sync}
		gprof lab2c gmon.out > prof_${sync}_${list}.txt
	done
done

# for iter in `seq 10000 10000 100000`;
# do
# 	echo ${iter}
# 	rm -f gmon.out
# 	./lab2c --t=1 --i=${iter} --l=1
# 	gprof lab2c gmon.out > prof_x_${iter}.txt
# done
