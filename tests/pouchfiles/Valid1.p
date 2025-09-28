IMPORT img_b
COPY file3.txt
RUN echo Marker-Valid1 > Valid1
RUN ls file3.txt ; echo abc ; ls > abc.abc
RUN cat abc.abc
COPY file2.txt
RUN echo abc > abc.abc ; ls > abd.abd ; cat abc.abc; cat abd.abd