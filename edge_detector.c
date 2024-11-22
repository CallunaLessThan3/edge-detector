#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>


// change the number of threads as you run your concurrency experiment
#define LAPLACIAN_THREADS (4)

/* Laplacian filter is 3 by 3 */
#define FILTER_WIDTH (3)
#define FILTER_HEIGHT (3)

#define RGB_COMPONENT_COLOR (255)

#define PIXEL_SIZE (3)
static const char PPM_SIG[] = "P6";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static const char USAGE[] = "Usage: edge_detector <image1.ppm> <image2.ppm> ...\n";

typedef struct {
    unsigned char r, g, b;
} PPMPixel;

// for each thread, given a parameter
struct parameter {
    PPMPixel *image;         // original image pixel data                 // const
    PPMPixel *result;        // filtered image pixel data                 // not const
    unsigned long int w;     // width of image                            // const
    unsigned long int h;     // height of image                           // const
    unsigned long int start; // starting point of work                    // const
    unsigned long int size;  // equal share of work (almost equal if odd) // const
};


struct file_name_args {
    char *input_file_name;      //e.g., file1.ppm
    char output_file_name[20];  //will take the form laplaciani.ppm, e.g., laplacian1.ppm
};


/* The total_elapsed_time is the total time taken by all threads
   to compute the edge detection of all input images. */
double total_elapsed_time = 0;


/* This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
   For each pixel in the input image, the filter is conceptually placed on top ofthe image with its origin lying on that pixel.
   The values of each input image pixel under the mask are multiplied by the corresponding filter values.
   Truncate values smaller than zero to zero and larger than 255 to 255.
   The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input. */
void *compute_laplacian_threadfn(void *params) {
    struct parameter *t_args = (struct parameter*)params;
    const int laplacian[FILTER_WIDTH][FILTER_HEIGHT] =
    {
        {-1, -1, -1},
        {-1,  8, -1},
        {-1, -1, -1}
    };


    size_t start_i = t_args->start * t_args->w;
    size_t end_i = (t_args->start + t_args->size) * t_args->w;

    // iterate over given pixels
    for (size_t i=start_i; i < end_i; i++) {
        int red = 0, green = 0, blue = 0;
        size_t iteratorImageWidth = i % t_args->w;
        size_t iteratorImageHeight = i / t_args->w;

        // iterate over filter
        for (size_t fw_i=0; fw_i < FILTER_WIDTH; fw_i++) {
            for (size_t fh_i=0; fh_i < FILTER_HEIGHT; fh_i++) {
                int x_coordinate = (iteratorImageWidth - FILTER_WIDTH / 2 + fw_i + t_args->w) % t_args->w;
                int y_coordinate = (iteratorImageHeight - FILTER_HEIGHT / 2 + fh_i + t_args->h) % t_args->h;

                red   += t_args->image[y_coordinate * t_args->w + x_coordinate].r * laplacian[fh_i][fw_i];
                green += t_args->image[y_coordinate * t_args->w + x_coordinate].g * laplacian[fh_i][fw_i];
                blue  += t_args->image[y_coordinate * t_args->w + x_coordinate].b * laplacian[fh_i][fw_i];
            }
        }

        if (red < 0)   red = 0;
        if (green < 0) green = 0;
        if (blue < 0)  blue = 0;

        if (red > 255)   red = 255;
        if (green > 255) green = 255;
        if (blue > 255)  blue = 255;

        t_args->result[iteratorImageHeight * t_args->w + iteratorImageWidth].r = red;
        t_args->result[iteratorImageHeight * t_args->w + iteratorImageWidth].g = green;
        t_args->result[iteratorImageHeight * t_args->w + iteratorImageWidth].b = blue;
    }

    pthread_exit(NULL);
    return NULL;
}


/* Apply the Laplacian filter to an image using threads.
   Each thread shall do an equal share of the work, i.e. work=height/number of threads.
   If the size is not even, the last thread shall take the rest of the work.
   Compute the elapsed time and store it in *elapsedTime (Read about gettimeofday).
   Return: result (filtered image) */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime) {
    struct parameter t_structs[LAPLACIAN_THREADS];
    pthread_t tids[LAPLACIAN_THREADS];
    const int work = h/LAPLACIAN_THREADS;
    PPMPixel *result = malloc(w * h * PIXEL_SIZE);
    int err;

    for (int i=0; i<LAPLACIAN_THREADS; i++) {
        struct parameter *t_args = &t_structs[i];
        pthread_t *tid = &tids[i];

        // assign params
        t_args->image = image;
        t_args->result = result;
        t_args->w = w;
        t_args->h = h;
        t_args->start = (work * i);
        t_args->size = (i == LAPLACIAN_THREADS-1) ? (h - t_args->start)  : (work);

        // make threads
        err = pthread_create(tid, NULL, compute_laplacian_threadfn, t_args);
        if (err != 0) {
            perror("error creating threads (compute_laplacian_threadfn)");
            exit(EXIT_FAILURE);
        }

        pthread_join(*tid, NULL);
    }


    // TODO: elapsedTime += x
    return result;
}


/* Create a new P6 file to save the filtered image in.
   Write the header block
   e.g. P6
        Width Height
        Max color value
   then write the image data.
   The name of the new file shall be "filename" (the second argument). */
void write_image(PPMPixel *image, char *filename, unsigned long int width, unsigned long int height) {
    const char *format = "%s\n%lu %lu\n%d\n";
    const int header_len = snprintf(NULL, 0, format, PPM_SIG, width, height, RGB_COMPONENT_COLOR);
    const size_t pixels = width * height;
    int count;

    const char *modes = "w";
    FILE *file = fopen(filename, modes);
    if (!file) {
        perror("error opening file");
        exit(EXIT_FAILURE);
    }


    /* header */
    char *header = calloc(header_len+1, 1);
    snprintf(header, header_len+1, format, PPM_SIG, width, height, RGB_COMPONENT_COLOR);

    count = fwrite(header, sizeof(char), header_len, file);
    if (count != header_len) {
        perror("error writing data");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    free(header);


    /* data */
    count = fwrite(image, PIXEL_SIZE, pixels, file);
    if (count != pixels) {
        perror("error writing data");
        fclose(file);
        exit(EXIT_FAILURE);
    }


    fclose(file);
    return;
}


/* Skips any whitespace and any comments in that whitespace
   whitespace: { ' ', '\n', '\r', '\t' }
   file's buffer pointer address will be the next non-whitespace character. */
static void skip_whitespace_comments(FILE* file) {
    char ch = fgetc(file);
    int done = 0;
    while (!done) {
        if (isspace(ch)) {
            // skips whitespace
            while (isspace(ch)) { ch = fgetc(file); }
        } else if (ch == '#') {
            // skips line after comment
            while (ch != '\n') { ch = fgetc(file); }
        } else {
            done = 1;
        }
    }

    fseek(file, -1, SEEK_CUR);
    return;
}


/* Open the filename image for reading, and parse it.
   Example of a ppm header:    //http://netpbm.sourceforge.net/doc/ppm.html
        P6                  -- image format
        # comment           -- comment lines begin with
        ## another comment  -- any number of comment lines
        200 300             -- image width & height
        255                 -- max color value

   Check if the image format is P6. If not, print invalid format error message.
   If there are comments in the file, skip them. You may assume that comments exist only in the header block.
   Read the image size information and store them in width and height.
   Check the rgb component, if not 255, display error message.
   Return: pointer to PPMPixel that has the pixel data of the input image (filename).
   The pixel data is stored in scanline order from left to right (up to bottom) in 3-byte chunks (r g b values for each pixel) encoded as binary numbers. */
PPMPixel *read_image(const char *filename, unsigned long int *width, unsigned long int *height) {
    const size_t fmt_len = 2;
    const size_t max_cv_len = 3;
    int count;

    const char *modes = "r";
    FILE *file = fopen(filename, modes);
    if (!file) {
        perror("error opening file");
        exit(EXIT_FAILURE);
    }


    /* get image format */
    char fmt[3] = "";
    count = fread(&fmt, sizeof(char), fmt_len, file);
    if (count != fmt_len) {
        perror("error reading image format");
        fclose(file);
        exit(EXIT_FAILURE);
    }


    /* check image format matches */
    if(strcmp(fmt, PPM_SIG)) {
        fprintf(stderr, "not a ppm file");
        exit(EXIT_FAILURE);
    }

    skip_whitespace_comments(file);


    /* get height and width */
    // get width
    char ch_w;
    size_t width_l = 0;
    while (ch_w = fgetc(file), !isspace(ch_w)) {
        width_l++;
    }

    fseek(file, -(width_l+1), SEEK_CUR);
    char *width_s = calloc(width_l+1, sizeof(char));

    count = fread(width_s, sizeof(char), width_l, file);
    if (count != width_l) {
        perror("error reading width");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    *width = strtoul(width_s, NULL, 10);
    free(width_s);

    skip_whitespace_comments(file);


    // get height
    char ch_l;
    size_t height_l = 0;
    while (ch_l = fgetc(file), !isspace(ch_l)) {
        height_l++;
    }

    fseek(file, -(height_l+1), SEEK_CUR);
    char *height_s = calloc(height_l+1, sizeof(char));

    count = fread(height_s, sizeof(char), height_l, file);
    if (count != height_l) {
        perror("error reading height");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    *height = strtoul(height_s, NULL, 10);
    free(height_s);

    skip_whitespace_comments(file);


    /* check max color value */
    int max_cv;
    char *max_cv_s = calloc(max_cv_len+1, sizeof(char));
    count = fread(max_cv_s, sizeof(char), max_cv_len, file);
    if (count != max_cv_len) {
        perror("error reading maximum color value");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    max_cv = atoi(max_cv_s);
    free(max_cv_s);

    if (max_cv != RGB_COMPONENT_COLOR) {
        fprintf(stderr, "not rgb");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    skip_whitespace_comments(file);


    /* read pixel data */
    const size_t pixels = (*height) * (*width);
    PPMPixel *img = malloc(pixels * PIXEL_SIZE);
    count = fread(img, PIXEL_SIZE, pixels, file);
    if (count != pixels) {
        perror("error reading image data");
        fclose(file);
        exit(EXIT_FAILURE);
    }


    fclose(file);
    return img;
}


/* The thread function that manages an image file.
   Read an image file that is passed as an argument at runtime.
   Apply the Laplacian filter.
   Update the value of total_elapsed_time.
   Save the result image in a file called laplaciani.ppm, where i is the image file order in the passed arguments.
   Example: the result image of the file passed third during the input shall be called "laplacian3.ppm". */
void *manage_image_file(void *args) {
    // might need to mutex this
    char *filename = (char*)args;
    printf("filename: %s\n", filename);
    return 0;
}


/* The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename[s]"
   It shall accept n filenames as arguments, separated by whitespace, e.g., ./a.out file1.ppm file2.ppm    file3.ppm
   It will create a thread for each input file to manage.
   It will print the total elapsed time in .4 precision seconds(e.g., 0.1234 s). */
int main(int argc, char *argv[]) {
    int err;
    if (argc < 2) {
        fprintf(stderr, "Error: Not enough arguments.\n%s", USAGE);
        exit(EXIT_FAILURE);
    }


    /* loop over each argument, create thread for each */
    pthread_t *tids = malloc(argc * sizeof(pthread_t));

    for (size_t i=1; i < argc; i++) {
        char *filename = argv[i];
        pthread_t *tid = &tids[i];

        err = pthread_create(tid, NULL, manage_image_file, filename);
        if (err != 0) {
            perror("error creating threads (manage_image_file)");
            exit(EXIT_FAILURE);
        }

        pthread_join(*tid, NULL);
    }

    free(tids);


    /*
    const char *in_filename = "image.ppm";
    const char *out_filename = "out_image.ppm";
    unsigned long width = 0;
    unsigned long height = 0;
    PPMPixel *image, *result;

    image = read_image(in_filename, &width, &height);
    result = apply_filters(image, width, height, NULL);
    write_image(result, (char*)out_filename, width, height);

    free(image);
    free(result);
    */

    return 0;
}

