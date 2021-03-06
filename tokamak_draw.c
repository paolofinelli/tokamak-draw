/*******************************************************
 *                 Tokamak draw
 * Draw surfaces and field-lines in a tokamak
 * Intended for drawing diagrams for presentation
 *
 * MIT LICENSE:
 *
 * Copyright (c) 2006-2008 B.Dudson, University of Oxford and UKAEA Fusion
 * Copyright (c) 2009-2010 B.Dudson, University of York
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Changelog:
 *
 *   Feb 2006: Ben Dudson, University of Oxford/UKAEA
 *             Adapted from bout_camera routines
 *
 *   Dec 2009: Ben Dudson, University of York
 *             Added scripting code
 * 
 *   Jan 2010: Ben Dudson, University of York
 *             Added GNU automake/autoconf build method
 *             
 ********************************************************/

#include <GL/glut.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "gl2ps.h"

#include "model.h"
#include "tokamak_draw.h"

/*********** GLOBALS *****************/

TCamera *dispview;
int win_width, win_height;

TColor planecolor, linecolor;

TModel drawmodel; /* The model being used */
char modelfile[256]; /* Filename for the model */

/*********** PROTOTYPES ****************/

TCamera *create_camera();
void redraw_camera();
void set_camera_pos(double R, double theta, double phi);
void move_camera(double dR, double dtheta, double dphi);
void set_camera_focus(double x, double y, double z);
void move_camera_focus(double up, double left);

void keyboard (unsigned char key, int x, int y);
void specialkey (int key, int x, int y);
void reshape(int w, int h);

float qromb(float (*func)(float, void*), float a, float b, void *params);
float trapzd(float (*func)(float, void*), float a, float b, int n, void *p);
void polint(float *xa, float *ya, float x, float *y, float *dy);

/** Drawing functions **/

void draw_planes(int n, float major, float minor, TColor *color);

/* Draw a m/n fieldline on a shaped flux-surface with elongation e and triangularity k */
void draw_shapeline(float R, float a, float e, float k, int m, int n, int N, TColor *color, float theta0);

void solid_surface(float R, float a, float e, float k, int N, TColor *color, float phi0, float phi1);
void draw_shapesurf(float R, float a, float e, float k, int m, int n, TColor *color, int N);
void draw_line(float q, float major, float minor,
	       float *theta, float *phi,
	       int N,
	       TColor *color,
	       float mode);
void draw_surface(int n, int m, float major, float minor, int M, float mode);


/************* CODE **************/

void draw_planes(int n, float major, float minor, TColor *color)
{
  int i;
  float z, dz;
  float r1, r2, x1, y1, x2, y2;

  dz = 2.0*PI / ((float) n);
  
  r1 = major - minor;
  r2 = major + minor;

  for(z=0.0,i=0;i<n;i++) {

    x1 = r1 * cos(z);
    y1 = r1 * sin(z);
    x2 = r2 * cos(z);
    y2 = r2 * sin(z);

    glBegin(GL_QUADS);
    glColor4f(color->r, color->g, color->b, color->alpha);
    glVertex3f(x1, -1.0*minor, y1);
    glVertex3f(x1, minor, y1);
    glVertex3f(x2, minor, y2);
    glVertex3f(x2, -1.0*minor, y2);
    glEnd();
    
    z += dz;
  }
}

float shapefunc(float theta, void *data)
{
  float *vals;
  float R, a, b;
  float ct;

  vals = (float*) data;
  R = vals[0];
  a = vals[1];
  b = vals[2];

  ct = cos(theta);
  return(1.0 / (a*ct - b*ct*ct + R) );
}

/* Draw a m/n fieldline on a shaped flux-surface with elongation e and triangularity k */
void draw_shapeline(float R, float a, float e, float k, int m, int n, int N, TColor *color, float theta0)
{
  int i, j;
  float dphi, phi;
  float theta;
  float b;
  float r, z, x, y;
  float ct;
  float alpha;
  float vals[3];

  b = a*( 2.0/(2.0 + k) - 1.0 );

  vals[0] = R;
  vals[1] = a;
  vals[2] = b;
  alpha = qromb(shapefunc, 0.0, 2.0*PI, (void*) vals);

  alpha = (((float) n) / ((float) m)) * 2.0*PI / alpha;

  phi = theta0;
  theta = 0.0;
  dphi = 2.0*PI / ((float) N);

  glBegin(GL_LINE_STRIP);
  glColor4f(color->r, color->g, color->b, 1.0);
  for(j=0;j<n;j++) {
    for(i=0;i<=N;i++) {
      /* Work out coordinates */
      ct = cos(theta);
      r = a*ct - b*ct*ct + R;
      x = r*cos(phi);
      y = r*sin(phi);
      z = a*(1.0 + e)*sin(theta);
      
      //printf("%d: (%f, %f), %f, (%f,%f,%f)\n", i, phi, theta, r, x, y, z);
      
      glVertex3f(x, z, y);
      
      /* Work out new theta */
      //printf("%d: %f, %f, %f -> %f, %f\n", i, b, alpha, r, dphi, r*dphi*alpha);
      phi += dphi;
      theta -= r*dphi/alpha;
    }
  }

  glEnd();
}

void solid_surface(float R, float a, float e, float k, int N, TColor *color, float phi0, float phi1)
{
  int i, j;
  float dphi, phi;
  float theta, dtheta;
  float b;
  float r1, z1, r2, z2;
  float ct;

  b = a*( 2.0/(2.0 + k) - 1.0 );

  theta = 0.0;
  dphi = (phi1 - phi0) / ((float) N);
  dtheta = 2.0*PI / ((float) N);

  ct = cos(theta);
  r2 = a*ct - b*ct*ct + R;
  z2 = a*(1.0 + e)*sin(theta);
  for(i=0;i<N;i++) {
    r1 = r2;
    z1 = z2;

    /* Work out coordinates */
    theta += dtheta;
    ct = cos(theta);
    r2 = a*ct - b*ct*ct + R;
    z2 = a*(1.0 + e)*sin(theta);

    //printf("(%f, %f) -> (%f, %f)\n", r1, z1, r2, z2);
    
    glBegin(GL_QUAD_STRIP);
    glColor4f(color->r, color->g, color->b, color->alpha);
    phi = 0.0;
    for(j=0;j<=N;j++) {
      glVertex3f(r1*cos(phi), z1, r1*sin(phi));
      glVertex3f(r2*cos(phi), z2, r2*sin(phi));
      phi += dphi;
    }
    glEnd();
    //return;
  }
  
}

void draw_shapesurf(float R, float a, float e, float k, int m, int n, TColor *color, int N)
{
  float dtheta, theta0;
  int i;

  dtheta = 2.0*PI / ((float) N);
  theta0 = 0.0;
  for(i=0;i<N;i++) {
    draw_shapeline(R, a, e, k, m, n, 100, color, theta0);
    theta0 += dtheta;
  }
}


/* Draw a field-line, starting at toroidal angle theta0, poloidal phi0
   with pitch q and minor radius r using N segments */
void draw_line(float q, float major, float minor,
	       float *theta, float *phi,
	       int N,
	       TColor *color,
	       float mode)
{
  int i;
  float dtheta, dphi;
  float r, x, y, z, mr, cp, mnr;

  dtheta = 2.0*PI / ((float) N);
  dphi = dtheta / q;

  mnr = 2.0;

  glBegin(GL_LINE_STRIP);
  glColor4f(color->r, color->g, color->b, 1.0);
  for(i=0;i<N;i++) {
    cp = cos(0.5*(*phi)); cp *= cp;
    mr = minor + mode*cp*cp*cp*cos(mnr*(*phi)*q - mnr*(*theta));

    r = major + mr*cos(*phi);
    x = r*cos(*theta);
    y = r*sin(*theta);
    z = mr*sin(*phi);
    
    glVertex3f(x, z, y);

    *theta += dtheta;
    *phi += dphi;
  }

  cp = cos(0.5*(*phi));
  mr = minor + mode*cp*cp*cp*cos(mnr*(*phi)*q - mnr*(*theta));

  r = major + mr*cos(*phi);
  x = r*cos(*theta);
  y = r*sin(*theta);
  z = mr*sin(*phi);
    
  glVertex3f(x, z, y);
  
  
  glEnd();
}

void draw_surface(int n, int m, float major, float minor, int M, float mode)
{
  float theta, phi, q, dphi;
  int i, j;

  q  = ((float) n) / ((float) m);

  dphi = 2.0*PI / ((float) M);

  for(i=0;i<M;i++) {
    theta = 0.0; phi = ((float) i) * dphi;
    for(j=0;j<n;j++)
      draw_line(q, major, minor, &theta, &phi, 100, &linecolor, mode);
  }
  
}

/************************* MAIN DRAWING ROUTINE ******************
 *
 * Define models as combination of surfaces and field-lines
 * NOTE: Currently no depth-sorting of transparent surfaces, so
 * won't look quite right.
 *****************************************************************/

void display()
{
  int i;
  TModelItem *item;

  glPushMatrix();

  //glClearDepth(0.0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  /* Draw something here */

  for(i=0;i<drawmodel.nitems;i++) {
    item = &drawmodel.item[i];
    switch(item->type) {
    case DRAW_LINE: {
      draw_shapesurf(item->major_radius, item->minor_radius,
		     item->elongation, item->triangularity, 
		     item->m, item->n, &item->color, item->number);
      break;
    };
    case DRAW_SOLID: {
      solid_surface( item->major_radius, item->minor_radius, 
		     item->elongation, item->triangularity,
		     item->number, &item->color, item->phi0, item->phi1);
      break;
    }
    case DRAW_PLANES: {
      draw_planes(item->number, item->major_radius, item->minor_radius, &item->color);
      break;
    }
    default: {
      fprintf(stderr,"ERROR: Unknown model item type. Ignoring\n");
    }
    };
  }
  
  /* Finish drawing */

  glFlush();
  glPopMatrix();
  glutSwapBuffers();
}

void init()
{
  glEnable( GL_DEPTH_TEST );

  glDepthFunc(GL_LESS);
  //glEnable ( GL_CULL_FACE );
  glShadeModel( GL_SMOOTH );

  glPolygonMode(GL_FRONT, GL_FILL);
  glPolygonMode(GL_BACK, GL_FILL);

  glMatrixMode(GL_MODELVIEW);

  glClearColor( 0.0, 0.0, 0.0, 0.0 );

}

int main(int argc, char **argv)
{
  printf("\n     Tokamak draw version %s\n", VERSION);
  printf("Ben Dudson, University of York <bd512@york.ac.uk>\n\n");
  

  /* Create the camera */
  dispview = create_camera();

  planecolor.r = 0.2;
  planecolor.g = 0.2;
  planecolor.b = 0.2;
  planecolor.alpha = 0.8;

  /* Clear the model */
  drawmodel.nitems = 0;
  
  if(argc > 1) {
    if(strcasecmp(argv[1], "example") == 0) {
      // Generate the input file
      model_write_example();
      strncpy(modelfile, "example.def", 255);
    }else {
      /* First argument is a model to load */
      strncpy(modelfile, argv[1], 255);
    }
  }else 
    strncpy(modelfile, "example.def", 255);
  if(model_load(&drawmodel, modelfile)) {
    fprintf(stderr, "Run '%s example' to generate an example input file 'example.def'\n", argv[0]);
  }

  glutInit (&argc, argv);
  glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize (640, 640);
  glutCreateWindow ("Tokamak draw");
  init();
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);
  glutSpecialFunc(specialkey);
  glutDisplayFunc(display);
  //glutIdleFunc(idle_func);
  glutMainLoop();

  return(0);
}


void reshape(int w, int h)
{
  glViewport (0, 0, w, h);
  glMatrixMode ( GL_PROJECTION );
  glLoadIdentity();
  
  if (h == 0) {
    gluPerspective (80, (float) w, 1.0, 1000.0 );
  }else {
    gluPerspective (80, (float) w / (float) h, 1.0, 1000.0);
  }
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();
  
  win_width = w;
  win_height = h;

  redraw_camera();
}

/**********************************************************************
 * Camera manipulations
 * 
 **********************************************************************/

TCamera *create_camera()
{
  TCamera *camera;

  camera = (TCamera*) malloc(sizeof(TCamera));

  camera->theta = 0.0;
  camera->phi = 0.0;
  camera->R = 5.0;

  /* centre on origin */
  camera->x = camera->y = camera->z = 0.0;
  return(camera);
}

void redraw_camera()
{
  double cx, cy, cz;

  /* Recalculate camera coordinates */
  cy = sin(dispview->phi) * dispview->R;
  cx = cos(dispview->phi) * sin(dispview->theta) * dispview->R;
  cz = -1.0 * cos(dispview->phi) * cos(dispview->theta) * dispview->R;

  /* Update record of camera location */
  dispview->cx = cx;
  dispview->cy = cy;
  dispview->cz = cz;

  glLoadIdentity();

  gluLookAt(cx, cy, cz,
	    dispview->x, dispview->y, dispview->z,
	    0.0, 1.0, 0.0); 
  
  glutPostRedisplay();
  
}

void set_camera_pos(double R, double theta, double phi)
{
  dispview->R = R;
  dispview->theta = theta;
  dispview->phi = phi;

  redraw_camera();
}

void move_camera(double dR, double dtheta, double dphi)
{
  double theta, phi;

  /* Change toroidal angle */
  theta = dispview->theta + dtheta;
  if(theta < 0.0)
    theta = 2.0*PI + theta;
  if(theta > 2.0*PI)
    theta = theta - 2.0*PI;
  dispview->theta = theta;

  phi = dispview->phi + dphi;
  if(phi > PI/2.0 - 0.1)
    phi = PI/2.0 - 0.1;
  if(phi < -PI/2.0 + 0.1)
    phi = -PI/2.0 + 0.1;
  dispview->phi = phi;

  dispview->R += dR;
  if(dispview->R < 1.5)
    dispview->R = 1.5;
  
  
  redraw_camera();
}


/* Set the camera to focus on a given point */
void set_camera_focus(double x, double y, double z)
{
  dispview->x = x;
  dispview->y = y;
  dispview->z = z;
  redraw_camera();
}

/* Move camera focus relative to current view */
void move_camera_focus(double up, double left)
{
  double c2px, c2pz;  /* Vector from camera to point */
  double r, theta;    /* Distance and angle from camera to point */

  /* Adjust 'left-right' */
  c2px = dispview->cx - dispview->x;
  c2pz = dispview->cz - dispview->z;

  r = sqrt(c2px*c2px + c2pz*c2pz);
  theta = atan2(c2pz, c2px);

  theta -= left / r;
  
  c2px = r * cos(theta);
  c2pz = r * sin(theta);

  dispview->x = dispview->cx - c2px;
  dispview->y += up;
  dispview->z = dispview->cz - c2pz;

  redraw_camera();
}


/**********************************************************************
 * Keyboard and Mouse handlers
 * 
 **********************************************************************/

void keyboard (unsigned char key, int x, int y)
{
  static int background = 0;
  static int transparency = 0;
  int opt;
  char file[256];
  GLint viewport[4];
  FILE *fp;
  
  static int format = GL2PS_PS;

  switch(key) {
  case 27: /* Escape key */
  case 'q':
    exit(0);
    break;
  case 'c': {
    /* Centre camera view on origin */
    set_camera_focus(0.0, 0.0, 0.0);
    break;
  }
  case 'C': {
    /* Centre camera view on origin and reset camera position */
    set_camera_focus(0.0, 0.0, 0.0);
    set_camera_pos(5.0, 0.0, 0.0);
    break;
  }
  case 'z':
  case '+':
    move_camera(-0.1, 0.0, 0.0); /* Zoom in */
    break;
  case 'x':
  case '-':
    move_camera(0.1,  0.0, 0.0); /* Zoom out */
    break;
  case 'b': {
    /* flip the background between black and white */
    if(background == 0) {
      glClearColor( 1.0, 1.0, 1.0, 0.0 );
      background = 1;
    }else {
      glClearColor( 0.0, 0.0, 0.0, 0.0 );
      background = 0;
    }
    glutPostRedisplay();
    break;
    }
  case 'a': {
    if(!transparency) {
      /* Enable transparency */

      glEnable(GL_BLEND);
      glDisable(GL_DEPTH_TEST); /* hack for when no sorting */
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      transparency = 1;
      printf("Transparency enabled\n");
    }else {
      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST); /* This is a hack for alpha*/
      transparency = 0;
      printf("Transparency disabled\n");
    }
    display(); /* re-draw */
    break;
  }
  case 'f': { // Change output format
    if     (format == GL2PS_PS)  format = GL2PS_EPS;
    else if(format == GL2PS_EPS) format = GL2PS_TEX;
    else if(format == GL2PS_TEX) format = GL2PS_PDF;
    else if(format == GL2PS_PDF) format = GL2PS_SVG;
    else if(format == GL2PS_SVG) format = GL2PS_PGF;
    else                         format = GL2PS_PS;
    printf("Print format changed to '%s'\n", gl2psGetFormatDescription(format));
    
    break;
  }
  case 'p': { // print to file
    
    opt = GL2PS_OCCLUSION_CULL;
    if(background)
      opt |= GL2PS_DRAW_BACKGROUND;
    
    sprintf(file, "draw_out.%s",gl2psGetFileExtension(format));

    viewport[0] = 0;
    viewport[1] = 0;
    viewport[2] = win_width;
    viewport[3] = win_height;

    fp = fopen(file, "wb");

    if(!fp){
      printf("Unable to open file %s for writing\n", file);
      break;
    }
    printf("Saving image to file %s... ", file);
    fflush(stdout);
    
    gl2psBeginPage(file, "pixie_draw", viewport, format, GL2PS_BSP_SORT, opt,
                   GL_RGBA, 0, NULL, 8, 8, 8, 
                   10*1024*1024, fp, file);

    display();
    
    gl2psEndPage();

    printf("Done!\n");
    fflush(stdout);

    break;
  }
  case 'l': {
    printf("Model file name to load: ");
    fgets(modelfile, 255, stdin);
    modelfile[255] = '\0';
    /* Need to strip final line return */
    modelfile[strlen(modelfile)-1] = '\0';
  }
  case 'r': {
    model_free(&drawmodel);
    model_load(&drawmodel, modelfile);
    break;
  }
  case '?':
  case 'h': { // help
    printf("\nCommands:\n");
    printf("  Move around with arrow keys\n");
    printf("  F1       - arrow keys rotate object\n");
    printf("  F2       - arrow keys move focus\n");
    printf("  ESC or q - exit\n");
    printf("  a        - enable/disable transparency\n");
    printf("  b        - flip background color\n");
    printf("  c        - centre camera on origin\n");
    printf("  C        - reset camera\n");
    printf("  f        - change output format\n");
    printf("  l        - Load a model\n");
    printf("  p        - print the current view to file\n");
    printf("  r        - Reload model from file\n");
    printf("  x or -   - zoom out\n");
    printf("  z or +   - zoom in\n");
    break;
  }
  };
}

void specialkey (int key, int x, int y)
{
  static int cameramode = 0;
  int mods;

  mods = glutGetModifiers();
  
  switch(key) {
  case GLUT_KEY_F1: {
    cameramode = 0; /* Move camera position */
    printf("Moving camera position mode\n");
    break;
  }
  case GLUT_KEY_F2: {
    cameramode = 1; /* Move focus position */
    printf("Moving camera focus point mode\n");
    break;
  }
  case GLUT_KEY_LEFT: {
    if(cameramode == 0) {
      move_camera(0.0, 0.1, 0.0);
    }else {
      move_camera_focus(0.0, 0.1);
    }
    break;
  }
  case GLUT_KEY_RIGHT: {
    if(cameramode == 0) {
      move_camera(0.0, -0.1, 0.0);
    }else {
      move_camera_focus(0.0, -0.1);
    }
    break;
  }
  case GLUT_KEY_UP: {
    if(cameramode == 0) {
      move_camera(0.0, 0.0, 0.1);
    }else if(cameramode == 1) {
      move_camera_focus(0.1, 0.0);
    }
    break;
  }
  case GLUT_KEY_DOWN: {
    if(cameramode == 0) {
      move_camera(0.0, 0.0, -0.1);
    }else if(cameramode == 1) {
      move_camera_focus(-0.1, 0.0);
    }
    break;
  }
  default:
    break;
  };
}

/*************************************************************************
 * Integrate a function using QROMB method
 *************************************************************************/

/* Fractional error, determined by extrapolation error */
#define EPS 1.0e-6
/* Maximum number of steps */
#define JMAX 20
#define JMAXP (JMAX+1)
/* Number of points to use in extrapolation */
#define K 5

/* Integrate function from a to b */
float qromb(float (*func)(float, void*), float a, float b, void *params)
{
  float ss, dss;
  float s[JMAXP], h[JMAXP+1];
  int j;
  
  h[0] = 1.0;
  for(j=0;j!=JMAX;j++) {
    s[j] = trapzd(func, a, b, j+1, params);
    if(j > K) {
      polint(&h[j-K], &s[j-K], 0.0, &ss, &dss);
      if(fabs(dss) <= EPS*fabs(ss)) return(ss);
      if(fabs(ss) < 1.0e-14) {
	printf("Value of function within rounding errors\n");
	return(0.0);
      }
    }
    h[j+1] = 0.25*h[j];
  }
  printf("Too many steps in function qromb\n");
  return(0.0);
}

#define FUNC(x, p) ((*func)(x, p))

/* Call with n=1 returns crudest estimate, subsequent calls improve accuracy by adding 2^(n-2) additional points */
float trapzd(float (*func)(float, void*), float a, float b, int n, void *p)
{
  float x, tnm, sum, del;
  static float s;
  int it, j;
  
  if(n == 1) {
    s = 0.5*(b-a)*(FUNC(a, p)+FUNC(b, p));
  }else {
    for(it=1,j=1;j<(n-1);j++) it <<= 1;
    tnm = it;
    del = (b-a)/tnm;
    x = a+0.5*del;
    for(sum=0.0,j=1;j<=it;j++,x+=del) sum += FUNC(x, p);
    s = 0.5*(s+(b-a)*sum/tnm); /* refine s */
  }
  return(s);
}

#define POL_N K

/* Polynomial interpolation/extrapolation: Input xa and ya, returns value y at x with error estimate dy */
void polint(float *xa, float *ya, float x, float *y, float *dy)
{
  int i, m, ns=0;
  float den, dif, dift, ho, hp, w;
  float c[POL_N], d[POL_N];
  
  dif = fabs(x - xa[0]);
  for(i=1;i<=POL_N;i++) {
    /* Find closest index */
    if((dift = fabs(x - xa[i-1])) < dif) {
      ns = i;
      dif = dift;
    }
    c[i-1] = ya[i-1];
    d[i-1] = ya[i-1];
  }
  
  *y = ya[ns--];
  for(m=1;m<POL_N;m++) {
    for(i=1;i<=(POL_N-m);i++) {
      ho = xa[i-1] - x;
      hp = xa[i+m-1] - x;
      w = c[i] - d[i-1];
      /* Two xa's within roundoff */
      if( (den = ho-hp) == 0.0) printf("problem in polint\n");
      den = w / den;
      d[i-1] = hp*den;
      c[i-1] = ho*den;
    }
    *y += (*dy=(2*ns < (POL_N-m) ? c[ns] : d[(ns--)-1]));
  }
}

