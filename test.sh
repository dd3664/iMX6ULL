#!/bin/bash

#declare -A site=(["google"]="www.google.com" ["runoob"]="www.runoob.com" ["taobao"]="www.taobao.com")
:<<EOF
declare -A site
site["google"]="www.google.com"
site["runoob"]="www.runoob.com"
#echo ${site[google]}
echo ${site[@]}

a=105
b=20
#val=`expr $a % $b`
if [ $a -gt $b ]
then
    echo "a>b"
fi
if [ $a -ne $b ]
then
    echo "a!=b"
fi


echo -e "Ok!\c"
echo -e "Ok!\nIt is a test" > myfile
echo `date`


printf "%-10s %-8s %-4s\n" 姓名 性别 体重
printf "%-10s %-8s %-4.2f\n" 郭靖 男 66.1234


a=10
b=20
#if (($a < $b))
if [ $a -lt $b ]
then
    echo "a < b"
fi


while read FILM
do
    echo "$FILM"
done


echo "请输入1到4之间的数字:"
echo "你输入的数字为"
read anum
case $anum in
    1) echo "你选择了1"
    ;;
    2) echo "你选择了2"
    ;;
    3) echo "你选择了3"
    ;;
    *) echo "你没有输入1到3之间的数字"
    ;;
esac


demoFun()
{
    echo "shell function"
}
demoFun
EOF

funwithParam()
{
    echo "P1: $1"
    echo "P2: $2"
    echo "P3: $3"
    echo "Pall: $*"
    return 1
}
funwithParam a b c 1 2 3
echo "$1"

#a=$((3*5))
#echo $a

:<< EOF
echo "aaa" >> myfile
echo "bbb" >> myfile
funwithParam a b c 1 2 3 >> myfile
EOF

echo "P1: $1"
