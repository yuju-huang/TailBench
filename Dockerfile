FROM ubuntu

ARG NUM_CPUS=40

RUN mkdir /TailBench; mkdir /TailBench/xapian; mkdir /TailBench/img-dnn
COPY ./xapian/ /TailBench/xapian/
COPY ./img-dnn/ /TailBench/img-dnn/
COPY ./tailbench.inputs.tgz /TailBench/
RUN cd /TailBench; tar zxvf tailbench.inputs.tgz
