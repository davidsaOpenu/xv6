IMPORT img_c
COPY file1.txt
RUN ls ; ls > abc.abc
RUN cat abc.abc
COPY file5.txt
RUN ls file5.txt ; ls > abd.abd ; ls > abc.abc
RUN echo veryLongCommandShouldStillBeOkay veryLongCommandShouldStillBeOkay veryLongCommandShouldStillBeOkay
RUN cd .. ; cat abc.abc
RUN echo Marker-Valid2 > Valid2