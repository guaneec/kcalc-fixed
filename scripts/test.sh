#!/usr/bin/env bash

CALC_DEV=/dev/calc
CALC_MOD=calc.ko

EVAL=./eval

test_op() {
    local expression=$1 
    echo "Testing " ${expression} "..."
    echo -ne ${expression}'\0' > $CALC_DEV
    # local tmp=$(cat $CALC_DEV)
    # echo "obase=2; $tmp" | bc
    $EVAL $(cat $CALC_DEV)
}

if [ "$EUID" -eq 0 ]
  then echo "Don't run this script as root"
  exit
fi

sudo rmmod -f calc 2>/dev/null
sleep 1

modinfo $CALC_MOD || exit 1
sudo insmod $CALC_MOD
sudo chmod 0666 $CALC_DEV
echo

# compares
test_op '1> -1'
test_op '1>=-1'
test_op '1< -1'
test_op '1<=-1'
test_op '1==-1'
test_op '1!=-1'
test_op '-1> 1'
test_op '-1>=1'
test_op '-1< 1'
test_op '-1<=1'
test_op '-1==1'
test_op '-1!=1'
test_op '-1> -1'
test_op '-1>=-1'
test_op '-1< -1'
test_op '-1<=-1'
test_op '-1==-1'
test_op '-1!=-1'


# multiply
test_op '6*7'
test_op '1.5*1.5'

# add
test_op '1980+1'

# sub
test_op '2019-1'

# div
test_op '42/6'
test_op '1/3'
test_op '1/3*6+2/4'
test_op '(1/3)+(2/3)'
test_op '(2145%31)+23'
test_op '0/0' # should be NAN_INT

# binary
test_op '(3%0)|0' # should be 0
test_op '1+2<<3' # should be (1 + 2) << 3 = 24
test_op '123&42' # should be 42
test_op '123^42' # should be 81

# parens
test_op '(((3)))*(1+(2))' # should be 9

# assign
test_op 'x=5, x=(x!=0)' # should be 1
test_op 'x=5, x = x+1' # should be 6

# fancy variable name
test_op 'six=6, seven=7, six*seven' # should be 42
test_op '小熊=6, 維尼=7, 小熊*維尼' # should be 42
test_op 'τ=1.618, 3*τ' # should be 3 * 1.618 = 4.854
test_op '$(τ, 1.618), 3*τ()' # shold be 3 * 1.618 = 4.854

# functions
test_op '$(zero), zero()' # should be 0
test_op '$(one, 1), one()+one(1)+one(1, 2, 4)' # should be 3
test_op '$(number, 1), $(number, 2+3), number()' # should be 5

# positional arguments
test_op '$(sqr,$1*$1),sqr(5)'
test_op '$(f, $1+$2), f(20, 3)'
test_op '$(f, $1+$2), f(100, f(20, 3))'
test_op '$(f, $1+$2), $(g, $1*$2), f(100, g(20, 3))'
test_op '$(f, $1+$2), $(g, $1*$2), f(100, g(20, f(3, 4)))'

# pre-defined function
test_op 'nop()'

# sqrt
test_op 'sqrt(-1)'
test_op 'sqrt(0)'
test_op 'sqrt(1)'
test_op 'sqrt(2)'
test_op 'sqrt(3)'
test_op 'sqrt(4)'
test_op 'sqrt(2147483647)'
test_op 'sqrt(2147483647 + (1 - (1 >> 32)))'

test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(2)'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(3)'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(4)'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(5)'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(1>>30)'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(1+(1>>31))'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(1+(1>>32))'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(2147483647+(1>>15))'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(2147483647+(1>>16))'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(2147483647+(1>>32))'
test_op '$(test_sqrt, a=$1, r=sqrt(a), r1=r+(1>>32), r*r<=a && (r1*r1>=a || r1*r1<0)), test_sqrt(2147483647 + (1 - (1 >> 32)))'

test_op 'sqrt(1/0)'
test_op 'sqrt(0/0)'

# sum
test_op 'sum(i, i, 1, 10)'
test_op 'sum(11 - i, i, 1, 10)'
test_op 'sum(i**2, i, 1, 10)'
test_op 'n=5, sum((i > 0) - (i < 0), i, -n, n)' # should be 0
test_op 'n=5, sum((i > 0) - (i < 0), i, -n+1, n)' # should be 1
test_op 'n=5, sum((i > 0) - (i < 0), i, -n, n-1)' # should be -1

# WARNING: hangs for a while, but should work
# test_op 'n=2147483647, sum((i > 0) - (i < 0), i, -n, n)' # should be 0
# test_op 'n=2147483647, sum((i > 0) - (i < 0), i, -n+1, n)' # should be 1
# test_op 'n=2147483647, sum((i > 0) - (i < 0), i, -n, n-1)' # should be -1

sudo rmmod calc

# epilogue
echo "Complete"
