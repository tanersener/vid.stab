#define NUM_RECTANGLES 100

inline static unsigned char randPixel(){
  return rand()%256;
}

inline static short randUpTo(short max){
  return rand()%max;
}

static void paintRectangle(unsigned char* buffer, const DSFrameInfo* fi, int x, int y, int sizex, int sizey, unsigned char color){
  if(x>=0 && x+sizex < fi->width && y>=0 && y+sizey < fi->height){
    int i,j;
    for(j=y; j < y+sizey; j++){
      for(i=x; i<x+sizex; i++){
	buffer[j*fi->width + i] = color;	
      }      
    }

  }
}


/// corr: correlation length of noise
static void fillArrayWithNoise(unsigned char* buffer, int length, float corr){
  unsigned char avg=randPixel();
  int i=0;
  if(corr<1) corr=1;
  float alpha = 1.0/corr;
  for(i=0; i < length; i++){
    buffer[i] = avg;
    avg = avg * (1.0-alpha) + randPixel()*alpha;
  }  
}

static Transform getTestFrameTransform(int i){
  Transform t;
  t.x = ( (i%2)==0 ? -1 : 1)  *i*5;
  t.y = ( (i%3)==0 ?  1 : -1) *i*5;
  t.alpha = (i<3 ? 0 : 1) * (i)*1*M_PI/(180.0);
  t.zoom = 0;
  return t;
}

static void generateFrames(unsigned char **frames, int num, const DSFrameInfo fi){
  int i;
  for(i=0; i<num; i++){
    frames[i] = (unsigned char*)malloc(fi.framesize);   
  }
  // first frame noise
  fillArrayWithNoise(frames[0], fi.framesize, 10);
  // add rectangles
  int k;
  for(k=0; k<NUM_RECTANGLES; k++){
    paintRectangle(frames[0],&fi,randUpTo(fi.width), randUpTo(fi.height),
		   randUpTo((fi.width>>4)+4),randUpTo((fi.height>>4)+4),randPixel());

  }
  
  TransformData td;
  assert(initTransformData(&td, &fi, &fi, "generate") == DS_OK);  
  td.interpolType=Zero;
  assert(configureTransformData(&td)== DS_OK);
  

  fprintf(stderr, "testframe transforms\n");

  for(i=1; i<num; i++){
    Transform t = getTestFrameTransform(i);
    fprintf(stderr, "%i, %6.4lf %6.4lf %8.5lf %6.4lf %i\n", 
	    i, t.x, t.y, t.alpha, t.zoom, t.extra);

    memcpy(frames[i], frames[i-1], fi.framesize);
    assert(transformPrepare(&td,frames[i])== DS_OK);
    assert(transformYUV_float(&td, t)== DS_OK);      
    memcpy(frames[i], td.dest, fi.framesize);

  }
  cleanupTransformData(&td);
}

static int readNumber (const char* filename, FILE *f)
{
  int c,n=0;
  for(;;) {
    c = fgetc(f);
    if (c==EOF) 
      ds_log_error("TEST", "unexpected end of file in '%s'", filename);
    if (c >= '0' && c <= '9') n = n*10 + (c - '0');
    else {
      ungetc (c,f);
      return n;
    }
  }
}


static void skipWhiteSpace (const char* filename, FILE *f)
{
  int c,d;
  for(;;) {
    c = fgetc(f);
    if (c==EOF) 
      ds_log_error("TEST", "unexpected end of file in '%s'", filename);

    // skip comments
    if (c == '#') {
      do {
	d = fgetc(f);
	if (d==EOF) 
	  ds_log_error("TEST", "unexpected end of file in '%s'", filename);
      } while (d != '\n');
      continue;
    }

    if (c > ' ') {
      ungetc (c,f);
      return;
    }
  }
}

int loadPGMImage(const char* filename, char ** framebuffer, DSFrameInfo* fi)
{
  FILE *f = fopen (filename,"rb");
  if (!f) { 
    ds_log_error("TEST", "Can't open image file '%s'", filename);
    return 0;
  }

  // read in header
  if (fgetc(f) != 'P' || fgetc(f) != '2')
    ds_log_error("TEST","image file ist not binary PGM (no P5 header) '%s'", filename);
  skipWhiteSpace (filename,f);

  // read in image parameters
  fi->width = readNumber (filename,f);
  skipWhiteSpace (filename,f);
  fi->height = readNumber (filename,f);
  skipWhiteSpace (filename,f);
  int max_value = readNumber (filename,f);

  // check values
  if (fi->width < 1 || fi->height < 1)
    ds_log_error("TEST", "bad image file '%s'", filename);
  if (max_value != 255)
    ds_log_error("TEST", "image file '%s' must have color range 255", filename);

  // read either nothing, LF (10), or CR,LF (13,10)
  int c = fgetc(f);
  if (c == 10) {
    // LF
  }
  else if (c == 13) {
    // CR
    c = fgetc(f);
    if (c != 10) ungetc (c,f);
  }
  else ungetc (c,f);


  // read in rest of data
  char* image_data = (char*) ds_malloc(fi->width*fi->height*3);
  *framebuffer = image_data;
  if (fread( image_data, fi->width*fi->height, 1, f) != 1){
    ds_log_error("TEST", "Can't read data from image file '%s'", filename);
    return 0;
  } 
  fclose (f);
  return 1;
}


int storePGMImage(const char* filename, unsigned char * image_data, DSFrameInfo fi ) {
  FILE *f = fopen (filename,"wb");
  if (!f) { 
    ds_log_error("TEST", "Can't open image file '%s'",  filename);
    return 0;
  }

  // write header
  fprintf(f,"P5\n");
  fprintf(f,"# CREATOR test suite of vid.stab\n");
  fprintf(f,"%i %i\n", fi.width, fi.height);
  fprintf(f,"255\n");

  // write data
  if (fwrite( image_data, fi.width*fi.height, 1, f) != 1){
    ds_log_error("TEST", "Can't write to image file '%s'", filename);
    return 0;
  } 
  fclose (f);
  return 1;
}
