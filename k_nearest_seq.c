#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <math.h>
#include <sys/time.h>
#include "simpletimer.h"
#include "parse.h"
#include "vec.h"

data_t features[ROWS][FEATURE_LENGTH] __attribute__((aligned(32)));
data_t timer_ref_MD,timer_ref_ED,timer_ref_CS;
data_t timer_opt_MD,timer_opt_ED,timer_opt_CS;

data_t abs_diff(data_t x, data_t y){
    data_t diff = x-y;
    return fabs(diff);
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

data_t squared_eucledean_distance(data_t *x,data_t *y, int length){
	data_t distance=0;
	int i = 0;
	for(i=0;i<length;i++){
		distance+= mult(abs_diff(x[i],y[i]),abs_diff(x[i],y[i]));
	}
	return distance;
}

data_t OPTsquared_eucledean_distance(data_t *x,data_t *y, int length){
    data_t distance=0;
    data_t z, z2, z3, z4, zz, zz2, zz3, zz4;
    int i = 0;
    int overig = length % 4;
    int length2 = length - overig;
    for(i=0;i<length2;i+=4){
        distance = distance +(((x[i]-y[i])*(x[i]-y[i]))+(((x[i+1]-y[i+1])*(x[i+1]-y[i+1]))+(((x[i+2]-y[i+2])*(x[i+2]-y[i+2]))+((x[i+3]-y[i+3])*(x[i+3]-y[i+3])))));
    }
    for(i = length2;i<length;i++){
        distance = distance +(x[i]-y[i]*x[i]-y[i]);
    }
    return distance;
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
    data_t sim=0;
    int i;
    int overig = length % 8;
    int length2 = length - overig;
    for(i=0;i<length2;i=i+8){
        sim += (x[i]*y[i])+(x[i+1]*y[i+1])+(x[i+2]*y[i+2])+(x[i+3]*y[i+3])+(x[i+4]*y[i+4])+(x[i+5]*y[i+5])+(x[i+6]*y[i+6])+(x[i+7]*y[i+7]);
    }
    for(; i<length;i++){
        sim += x[i]*y[i];
    }
    sim = sim / (OPTnorm(x,FEATURE_LENGTH)*OPTnorm(y,FEATURE_LENGTH));
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
    unsigned int i, a_found, b_found;
    data_t *a_res = a(lookFor, &a_found);
    data_t *b_res = b(lookFor, &b_found);

    for(i=0;i<ROWS-1;i++){
        if(a_res[i]!=b_res[i])
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
