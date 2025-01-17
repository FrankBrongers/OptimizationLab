#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <math.h>
#include <sys/time.h>
#include "simpletimer.h"
#include "parse.h"
#include "vec.h"

#include <smmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
/* Number of bytes in a vector */
/* Check the extensions of your machine to decide!
 * E.g. SSE4 = 128 bits, AVX = 256 bits*/

#define VBYTES 16    // DAS4 = SSE4.2 = 128 bits

/* Number of elements in a vector */
#define VSIZE VBYTES/sizeof(data_t)


/* Vector data type */
typedef data_t vec_t __attribute__ ((vector_size(VBYTES)));

typedef union {
    vec_t v;
    data_t d[VSIZE];
} pack_t;


data_t features[ROWS][FEATURE_LENGTH] __attribute__((aligned(32)));
data_t timer_ref_MD,timer_ref_ED,timer_ref_CS;
data_t timer_opt_MD,timer_opt_ED,timer_opt_CS;

data_t  abs_diff(data_t x, data_t y){
    data_t diff = x-y;
    return fabs(diff);
}

vec_t simd_abs_diff(vec_t x, vec_t y) {
  pack_t temp;
  int i;

  temp.v = x-y;
  for (i=0; i<VSIZE; i++) temp.d[i]=fabs(temp.d[i]);
  return temp.v;
}

data_t mult(data_t x,data_t y){
    data_t m = x*y;
    return m;
}

data_t manhattan_distance(data_t *x, data_t *y, int length){
    data_t distance=0;
    int i =0;
    for(i=0;i<length;i++){
        distance+=abs_diff(x[i],y[i]);
    }
    return distance;
}

inline __m128d abs_pd(__m128d x) {
             __m128d sign_mask = _mm_set1_pd(-0.); // -0. = 1 << 63
            return _mm_andnot_pd(sign_mask, x); // !sign_mask & x
}

inline __m256d abs256_pd(__m256d x) {
            __m256d sign_mask = _mm256_set1_pd(-0.);
            return _mm256_andnot_pd(sign_mask,x);
}


data_t simd_manhattan_distance_intr(data_t *x, data_t *y, int length){
    int i =0;
    data_t result=0;
    __m128d vx,vy,sub,abs_diff;
    __m128d distance=_mm_set_pd(0.0,0.0);
    __m128d zero= _mm_set_pd(0.0,0.0);
    for(i=0;i<length;i+=VSIZE){
         vx = _mm_load_pd(x+i);
         vy = _mm_load_pd(y+i);
         sub = _mm_sub_pd(vx,vy);
         abs_diff= abs_pd(sub);
         distance=_mm_add_pd(distance,abs_diff);

    }
    distance = _mm_hadd_pd(distance,zero);
    result = _mm_cvtsd_f64(distance);
    while (i < length) {
        result += fabs(*(x+i) - *(y+i));
        i++;
    }
	return result;
}

data_t simd_avx2_manhattan_distance_intr(data_t *x, data_t *y, int length){
    int i=0;
    data_t result=0;
    __m256d vx,vy,sub,abs_diff;
    __m256d distance=_mm256_set_pd(0.0,0.0,0.0,0.0);
    __m256d zero= _mm256_set_pd(0.0,0.0,0.0,0.0);
    for(i=0;i<length;i+=4){
         vx = _mm256_load_pd(x+i);
         vy = _mm256_load_pd(y+i);
         sub = _mm256_sub_pd(vx,vy);
         abs_diff= abs256_pd(sub);
         distance=_mm256_add_pd(distance,abs_diff);
    }
    distance = _mm256_hadd_pd(distance,zero);
    double *v_distance = (double*) &distance;
    result = v_distance[0]+v_distance[2];

    while (i < length) {
        result += fabs(*(x+i) - *(y+i));
        i++;
    }
	return result;
}

data_t squared_eucledean_distance(data_t *x,data_t *y, int length){
	data_t distance=0;
	int i = 0;
	for(i=0;i<length;i++){
		distance+= mult(abs_diff(x[i],y[i]),abs_diff(x[i],y[i]));
	}
	return distance;
}

data_t OPTsquared_eucledean_distance(data_t *x,data_t *y, int length){
    int i=0;
    data_t result=0;
    __m256d vx,vy,sub,diff;
    __m256d distance=_mm256_set_pd(0.0,0.0,0.0,0.0);
    for(i=0;i<length;i+=4){
         vx = _mm256_load_pd(x+i);
         vy = _mm256_load_pd(y+i);
         sub = _mm256_sub_pd(vx,vy);
         diff= abs256_pd(sub);
         diff= _mm256_mul_pd(diff, diff);
         distance=_mm256_add_pd(distance,diff);
    }

    double *v_distance = (double*) &distance;
    result = v_distance[0]+v_distance[1]+v_distance[2]+v_distance[3];

    while (i < length) {
        result += mult(fabs(x[i]-y[i]),fabs(x[i]-y[i]));
        i++;
    }
	return result;
}

data_t norm(data_t *x, int length){
    data_t n = 0;
    int i=0;
    for (i=0;i<length;i++){
        n += mult(x[i],x[i]);
    }
    n = sqrt(n);
    return n;
}

data_t OPTnorm(data_t *x, int length){
    data_t n = 0;
    int i;
    int overig = length % 8;
    int length2 = length - overig;
    for (i=0;i<length2;i=i+8){
        n += (x[i]*x[i])+(x[i+1]*x[i+1])+(x[i+2]*x[i+2])+(x[i+3]*x[i+3])+(x[i+4]*x[i+4])+(x[i+5]*x[i+5])+(x[i+6]*x[i+6])+(x[i+7]*x[i+7]);
    }
    for(; i<length;i++){
        n += x[i]*x[i];
    }
    n = sqrt(n);
    return n;
}

data_t cosine_similarity(data_t *x, data_t *y, int length){
    data_t sim=0;
    int i=0;
    for(i=0;i<length;i++){
        sim += mult(x[i],y[i]);
    }
    sim = sim / mult(norm(x,FEATURE_LENGTH),norm(y,FEATURE_LENGTH));
    return sim;
}

data_t OPTcosine_similarity(data_t *x, data_t *y, int length){
    int i=0;
    data_t sim=0;
    __m256d vx,vy,sub,diff;
    __m256d distance=_mm256_set_pd(0.0,0.0,0.0,0.0);
    for(i=0;i<length;i+=4){
         vx = _mm256_load_pd(x+i);
         vy = _mm256_load_pd(y+i);
         diff= _mm256_div_pd(diff, diff);
         distance=_mm256_add_pd(distance,diff);
    }

    double *v_distance = (double*) &distance;
    sim = v_distance[0]+v_distance[1]+v_distance[2]+v_distance[3];

    while (i < length) {
        sim += mult(fabs(x[i]-y[i]),fabs(x[i]-y[i]));
        i++;
    }
    sim = sim / mult(OPTnorm(x,FEATURE_LENGTH),OPTnorm(y,FEATURE_LENGTH));
	return sim;
}


data_t *ref_classify_MD(unsigned int lookFor, unsigned int *found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

	timer_start(&stv);
	min_distance = manhattan_distance(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
	for(i=1;i<ROWS-1;i++){
		current_distance = manhattan_distance(features[lookFor],features[i],FEATURE_LENGTH);
        	result[i]=current_distance;
		if(current_distance<min_distance){
			min_distance=current_distance;
			closest_point=i;
		}
	}
    timer_ref_MD = timer_end(stv);
    printf("Calculation using reference MD took: %10.6f \n", timer_ref_MD);
    *found=closest_point;
    return result;
}

//NO NEED to modify this function!
data_t *opt_classify_MD(unsigned int lookFor, unsigned int *found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

        timer_start(&stv);
        //min_distance = simd_manhattan_distance_intr(features[lookFor],features[0],FEATURE_LENGTH);
        min_distance = simd_avx2_manhattan_distance_intr(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
        for(i=1;i<ROWS-1;i++){
                //current_distance =simd_manhattan_distance_intr(features[lookFor],features[i],FEATURE_LENGTH);
                current_distance =simd_avx2_manhattan_distance_intr(features[lookFor],features[i],FEATURE_LENGTH);
                result[i]=current_distance;
                if(current_distance<min_distance){
                        min_distance=current_distance;
                        closest_point=i;
                }
        }
    timer_opt_MD = timer_end(stv);
    printf("Calculation using optimized MD took: %10.6f \n", timer_opt_MD);
    *found = closest_point;
    return result;
}

//Don't touch this function
data_t *ref_classify_ED(unsigned int lookFor, unsigned int *found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

	timer_start(&stv);
	min_distance = squared_eucledean_distance(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
	for(i=1;i<ROWS-1;i++){
		current_distance = squared_eucledean_distance(features[lookFor],features[i],FEATURE_LENGTH);
        result[i]=current_distance;
		if(current_distance<min_distance){
			min_distance=current_distance;
			closest_point=i;
		}
	}
    timer_ref_ED = timer_end(stv);
    printf("Calculation using reference ED took: %10.6f \n", timer_ref_ED);
    *found = closest_point;
    return result;
}

//Modify this function
data_t *opt_classify_ED(unsigned int lookFor, unsigned int *found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

	timer_start(&stv);
    //FROM HERE
	min_distance = OPTsquared_eucledean_distance(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
	for(i=1;i<ROWS-1;i++){
		current_distance = OPTsquared_eucledean_distance(features[lookFor],features[i],FEATURE_LENGTH);
        	result[i]=current_distance;
		if(current_distance<min_distance){
			min_distance=current_distance;
			closest_point=i;
		}
	}
    //TO HERE
    timer_opt_ED = timer_end(stv);
    printf("Calculation using optimized ED took: %10.6f \n", timer_opt_ED);
    *found = closest_point;
    return result;
}


//Don't touch this function
data_t *ref_classify_CS(unsigned int lookFor, unsigned int* found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

	timer_start(&stv);
	min_distance = cosine_similarity(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
	for(i=1;i<ROWS-1;i++){
		current_distance = cosine_similarity(features[lookFor],features[i],FEATURE_LENGTH);
        	result[i]=current_distance;
		if(current_distance>min_distance){
			min_distance=current_distance;
			closest_point=i;
		}
	}
    timer_ref_CS = timer_end(stv);
    printf("Calculation using reference CS took: %10.6f \n", timer_ref_CS);
    *found = closest_point;
    return result;
}

//Modify this function
data_t *opt_classify_CS(unsigned int lookFor, unsigned int *found) {
    data_t *result =(data_t*)malloc(sizeof(data_t)*(ROWS-1));
    struct timeval stv, etv;
    int i,closest_point=0;
    data_t min_distance,current_distance;

    timer_start(&stv);

    //MODIFY FROM HERE
	min_distance = OPTcosine_similarity(features[lookFor],features[0],FEATURE_LENGTH);
    	result[0] = min_distance;
	for(i=1;i<ROWS-1;i++) {
		current_distance = OPTcosine_similarity(features[lookFor],features[i],FEATURE_LENGTH);
        	result[i]=current_distance;
		if(current_distance>min_distance){
			min_distance=current_distance;
			closest_point=i;
		}
	}
    //TO HERE
    timer_opt_CS = timer_end(stv);
    printf("Calculation using optimized CS took: %10.6f \n", timer_opt_CS);
    *found = closest_point;
    return result;
}

typedef data_t (*(*classifying_funct)(unsigned int lookFor, unsigned int* found));

int check_correctness(classifying_funct a, classifying_funct b, unsigned int lookFor, unsigned int *found) {
    unsigned int r=1, i, a_found, b_found;
    data_t *a_res = a(lookFor, &a_found);
    data_t *b_res = b(lookFor, &b_found);

    for(i=0;i<ROWS-1;i++)
        if(fabs(a_res[i]-b_res[i])>0.001) {
           return 0;
 	}
    if (a_found != b_found) return 0;
    *found=a_found;
    return 1;
}

int main(int argc, char **argv){
	char* dataset_name=DATASET;
	int i,j;
        struct timeval stv, etv;
	unsigned int lookFor=ROWS-1, located;
	//PARSE CSV

	//holds the information regarding author and title
	char metadata[ROWS][2][20];

	timer_start(&stv);
	parse_csv(dataset_name, features, metadata);
	printf("Parsing took %9.6f s \n\n", timer_end(stv));

    printf("Classifying using MD:");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n",lookFor,metadata[lookFor][0],metadata[lookFor][1]);
    if(check_correctness(ref_classify_MD,opt_classify_MD, lookFor, &located)){
        printf("opt_classify_MD is correct, speedup: %10.6f\n\n",timer_ref_MD/timer_opt_MD);
    }
    else
        printf("opt_classify_MD is incorrect! \n"); // , speedup: %10.6f\n\n",timer_ref_MD/timer_opt_MD);
    printf("Best match: ");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n\n",located,metadata[located][0],metadata[located][1]);

    printf("Classifying using ED:");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n",lookFor,metadata[lookFor][0],metadata[lookFor][1]);
    if(check_correctness(ref_classify_ED,opt_classify_ED, lookFor, &located)) {
        printf("opt_classify_ED is correct, speedup: %10.6f\n\n",timer_ref_ED/timer_opt_ED);
    }
    else
        printf("opt_classify_ED id incorrect!\n\n");
    printf("Best match: ");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n\n",located,metadata[located][0],metadata[located][1]);

    printf("Classifying using CS (cosine similarity):");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n",lookFor,metadata[lookFor][0],metadata[lookFor][1]);
    if(check_correctness(ref_classify_CS,opt_classify_CS, lookFor, &located)) {
        printf("opt_classify_CS is correct, speedup: %10.6f\n\n",timer_ref_CS/timer_opt_CS);
    }
    else
        printf("opt_classify_CS id incorrect!\n\n");
    printf("Best match: ");
    printf("<Record %d, author =\"%s\", title=\"%s\">\n\n",located,metadata[located][0],metadata[located][1]);

}
