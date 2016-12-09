for j in `seq 100 2500 50001`; 
do
	echo "./lab2a --iterations=${j} --thread=1"
	(>&2 echo "./lab2a --iterations=${j} --thread=1")
	for i in `seq 1`; 
	do
		./lab2a --iterations=${j} --thread=1
	done	
done

# for k in m s c x; 
# do
# 	for j in `seq 32`; 
# 	do
# 		echo "./lab2a --iterations=100000 --thread=${j} --sync=${k}"
# 		(>&2 echo "./lab2a --iterations=100000 --thread=${j} --sync=${k}")
# 		for i in `seq 1`; 
# 		do
# 			./lab2a --iterations=100000 --thread=${j} --sync=${k}
# 		done	
# 	done
# done