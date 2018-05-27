#!/bin/bash

if [ $# -lt 1 ]; then
			echo "usage: test cmd [iterations]"
			fi
			cmd=$1

			cd root

			iter=1000
			if [ $# -eq 2 ]; then
						iter=$2
						fi

						echo "executing $cmd"
						counter=1
						tmpfile=mytmp
						while [ $iter -ge $counter ]; do
									sys161 kernel "$cmd" > $tmpfile
											if [ `wc -l $tmpfile | cut -f1 -d' '` -lt 40 ]; then
																echo "failed"
																				exit 1
																						fi
																								printf "\r$counter/$iter"
																										let "counter=counter+1"
																										done

																										echo
																										exit 0

