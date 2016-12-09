# for j in 10 20 50 100 200 500 1000 2000 5000; 
# do
# 	echo "./lab2b --iterations=${j} --thread=1"
# 	(>&2 echo "./lab2b --iterations=${j} --thread=1")
# 	for i in `seq 1`; 
# 	do
# 		./lab2b --iterations=${j} --thread=1
# 	done	
# done

for k in m s; 
do
	for j in 1 2 4 6 8 10 12 14 16; 
	do
		# echo "./lab2b --iterations=1000 --thread=${j} --sync=${k}"
		# (>&2 echo "./lab2b --iterations=1000 --thread=${j} --sync=${k}")
		# for i in `seq 1`; 
		# do
		# 	./lab2b --iterations=1000 --thread=${j} --sync=${k}
		# done
		for i in `seq 10`; do ./lab2b --t=${j} --i=1000 --s=${k}; done | grep 'per oper' | sort | awk '{a+=substr($3,1,length($3)-1)} END{print a/NR}'	
	done
done