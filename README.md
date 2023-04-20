# MediaFormatConverter
This is a C command code to convert the media format
Currently, the code support the convert list:
1. .aac audio file to mp4 with opus audio format 
2. .mp4 file includes AAC audio to mp4 with Opus audio format
3. .wav file includes PCM audio to mp4 with Opus audio format

# Platform preparation
1. Use Ubuntu LTS 22.04
2. git clone https://github.com/WalkerCode1989/MediaFormatConverter
3. Run the "prepare_ffmpeg.sh" for ffmpeg clone and make install  
./prepare_ffmpeg.sh

# Code compilation
mkdir build  
cd build  
cmake ../  
make

# Program testing
media_converter {input file name} {output file name}  
ex:  
media_converter aac_sample.aac aactoopus.mp4  
