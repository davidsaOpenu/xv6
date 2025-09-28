IMPORT img_b
RUN /ls
RUN /ls .build
COPY all_images
RUN echo hi > all_images ; echo Marker-Valid3 > Valid3
