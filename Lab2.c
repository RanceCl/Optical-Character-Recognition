
/*
	ECE 4310
	Lab 2
	Roderick "Rance" White
	
	This program 
	1) Reads a PPM image, template image, and ground truth files.
	2) Calculates the matched-spatial filter (MSF) image.
	3) Normalize the MSF image to 8-bits. 
	4) For a range of T, loop through: 
		a) Threshold at T the normalized MSF image to create a binary image.
		b) Loop through the ground truth letter locations.
			i) Check a 9 x 15 pixel area centered at the ground truth location. If
				 any pixel in the msf image is greater than the threshold, consider
				 the letter “detected”. If none of the pixels in the 9 x 15 area are
				 greater than the threshold, consider the letter “not detected”.
		c) Categorize and count the detected letters as FP (“detected” but the letter is
			 not ‘e’) and TP (“detected” and the letter is ‘e’).
		d) Output the total FP and TP for each T. 


	The program also demonstrates how to time a piece of code.
	To compile, must link using -lrt  (man clock_gettime() function).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

unsigned char *Image_Read(char *FileName, char *header, int *r, int *c, int *b);
void Image_Write(unsigned char *image, char *FileName, int ROWS, int COLS);
int *Zero_Mean_Center_Image(unsigned char *image, int ROWS, int COLS);
int *Convolve_Two_Images(unsigned char *image1, int *image2, int ROWS, int COLS, 
	int TROWS, int TCOLS);
void Find_Max_and_Min(int *MSF, int ROWS, int COLS, int *minimumV, int *maximumV);
unsigned char *Normalize_Image(int *image, int ROWS, int COLS, int NewMin, int NewMax);
void ROC_Evalutation(char *FileName, char SearchedLetter, unsigned char *image, int ROWS, int COLS, 
	int TROWS, int TCOLS);

	

int main()
{
	unsigned char		*InputImage, *TemplateImage, *NormalizedImage;
	int 						*MSF, *ZeroMeanCenter;
	char						header[320];
	int							ROWS,COLS,BYTES;					// The information for the input image
	int							TROWS,TCOLS,TBYTES;				// The information for the template image


	/* Read in the images */
	InputImage = Image_Read("parenthood.ppm", header, &ROWS, &COLS, &BYTES);
	TemplateImage = Image_Read("parenthood_e_template.ppm", header, &TROWS, &TCOLS, &TBYTES);

	/* Find the MSF */
	ZeroMeanCenter = Zero_Mean_Center_Image(TemplateImage, TROWS, TCOLS);
	MSF = Convolve_Two_Images(InputImage, ZeroMeanCenter, ROWS, COLS, TROWS, TCOLS);
	NormalizedImage = Normalize_Image(MSF, ROWS, COLS, 0, 255);
	Image_Write(NormalizedImage, "NormalizedMSF.ppm", ROWS, COLS);
	
	/* ROC Evaluation */
	ROC_Evalutation("parenthood_gt.txt", 'e', NormalizedImage, ROWS, COLS, TROWS, TCOLS);

}



/* This function serves to read and open the image, given it's name. 
 * It will return the values within the file, the number of rows, the number of columns, and
 * the number of bytes.
 */
unsigned char *Image_Read(char *FileName, char *header, int *r, int *c, int *b)
{
	FILE						*fpt;
	unsigned char		*image;
	int							ROWS,COLS,BYTES;
	
	/* read image */
	if ((fpt=fopen(FileName,"rb")) == NULL)
	{
		printf("Unable to open %s for reading\n", FileName);
		exit(0);
	}

	/* read image header (simple 8-bit greyscale PPM only) */
	fscanf(fpt,"%s %d %d %d",header,&COLS,&ROWS,&BYTES);	
	if (strcmp(header,"P5") != 0  ||  BYTES != 255)
	{
		printf("Not a greyscale 8-bit PPM image\n");
		exit(0);
	}
	
	/* allocate dynamic memory for image */
	image=(unsigned char *)calloc(ROWS*COLS,sizeof(unsigned char));
	header[0]=fgetc(fpt);	/* read white-space character that separates header */
	fread(image,1,COLS*ROWS,fpt);
	fclose(fpt);
	
	*r = ROWS;		//Return number of rows for the image
	*c = COLS;		//Return number of columns for the image
	*b = BYTES;
	
	return image;
}


/* This function serves to write the image to the appropriate file for more concise code */
void Image_Write(unsigned char *image, char *FileName, int ROWS, int COLS)
{
	FILE						*fpt;
	fpt=fopen(FileName,"w");
	fprintf(fpt,"P5 %d %d 255\n",COLS,ROWS);
	fwrite(image,COLS*ROWS,1,fpt);
	fclose(fpt);
}


/* The following function takes in an image, the rows and columns in it, and finds the zero-mean 
 * center of whatever image is sent to it.
 */
int *Zero_Mean_Center_Image(unsigned char *image, int ROWS, int COLS)
{
	int i;
	int sum=0;
	int meanvalue;
	int *zero_mean_center;
	
	/* allocate dynamic memory for the centered template */
	zero_mean_center=(int *)calloc(ROWS*COLS,sizeof(int));
	
	/* Find the mean of the template values. */
	//Loop through the values of the image to add them together.
	for (i=0; i<(ROWS*COLS); i++)
		sum += image[i];							// Adds together every value in the template
	meanvalue = sum/(ROWS*COLS);		// Divides the sum by the total number of values for the average
	
	/* Zero-mean center each value of the template */
	for (i=0; i<(ROWS*COLS); i++)
		zero_mean_center[i] = image[i]-meanvalue;		// Subtract every value of the template by the mean
	return zero_mean_center;
}

/* Finds the Convolution of Two Images
 * For this lab, this will mostly be used for finding the MSF 
 */
int *Convolve_Two_Images(unsigned char *image1, int *image2, int ROWS, int COLS, 
	int TROWS, int TCOLS)
{
	int r, c, dr, dc, Wr, Wc, sum;
	int *imageConvl;
	Wr = TROWS/2;		//Half the size of the template rows.
	Wc = TCOLS/2;		//Half the size of the template colunms.
	
	/* allocate dynamic memory for resulting MSF */
	imageConvl=(int *)calloc(ROWS*COLS,sizeof(int));
	
	//Loop through every row,column coordinate and convolve for each
	for(r=Wr; r<ROWS-Wr; r++)
	{
		for(c=Wc; c<=COLS-Wc; c++)
		{
			//Convolve for each value
			sum=0;
			for(dr=-Wr; dr<TROWS-Wr; dr++)
			{
				for(dc=-Wc; dc<TCOLS-Wc; dc++)
				{
					//Adding the proper convolution for each coordinate
					sum+= image1[(r+dr)*COLS+(c+dc)] * image2[(dr+Wr)*TCOLS+(dc+Wc)];
				}
			}
			imageConvl[r*COLS+c]=sum;
		}
	}
	return imageConvl;
}

void Find_Max_and_Min(int *MSF, int ROWS, int COLS, int *minimumV, int *maximumV)
{
	int i, min, max;
	
	//Set initial values
	min = MSF[0];
	max = MSF[0];
	
	//Compare all the values in the image to find the maximum and minimum values.
	for(i=0; i<(ROWS*COLS); i++)
	{
		//If the value is less than the current minimum, it is the new minimum
		if(min>MSF[i])
			min = MSF[i];
		
		//If the value is more than the current maximum, it is the new maximum
		if(max<MSF[i])
			max = MSF[i];
	}
	
	//Set the values at the end to return them both;
	*minimumV = min;
	*maximumV = max;
}

/* The following takes in an image, its size, current min and max values, and desired min and max
 * values and normalizes the image according to these values.
 */
unsigned char *Normalize_Image(int *image, int ROWS, int COLS, int NewMin, int NewMax)
{
	int i, Min, Max;
	unsigned char *imageNorm;
	
	Find_Max_and_Min(image, ROWS, COLS, &Min, &Max);
	
	/* allocate dynamic memory for image */
	imageNorm=(unsigned char *)calloc(ROWS*COLS,sizeof(unsigned char));
	
	//Loop through all the values and normalize them using it and the new and old min/max
	for(i=0; i<(ROWS*COLS); i++)
		imageNorm[i] = (image[i]-Min)*(NewMax-NewMin)/(Max-Min)+NewMin;
	return imageNorm;	
}

void ROC_Evalutation(char *FileName, char SearchedLetter, unsigned char *image, int ROWS, int COLS, 
	int TROWS, int TCOLS)
{
	FILE	*fpt, *ROCTable;
	int *Matches;
	int	i,tp,tn,fp,fn;
	int	system;
	char gt;
	int GTRow, GTCol;
	
	int r, c, Wr, Wc, T, TMin, TMax;
	Wr = TROWS/2;		//Half the size of the template rows.
	Wc = TCOLS/2;		//Half the size of the template colunms.

	//Open a spreadsheet for the ROC curve inputs
	ROCTable=fopen("ROCTable.csv","w");

	fprintf(ROCTable, "Threshold,TP,TN,FP,FN,TPR,FPR,PPV,sensitivity,specificity\n");
	printf("Please two numbers between 0 and 255 to act as boundaries for the threshold: ");
	scanf("%i %i", &TMin, &TMax);
	printf("\n");
	
	//Change minimum threshold if it is beyond possible range
	if(TMin<0 || TMin>255)
	{
		printf("Minimum is not possible, defaulting to 0.\n");
		TMin=0;
	}
	
	//Change maximum threshold if it is beyond possible range
	if(TMax<0 || TMax>255)
	{
		printf("Maximum is not possible, defaulting to 255.\n");
		TMax=255;
	}
	
	//Switch the min and max if the min is greater than the max
	if(TMin > TMax)
	{
		T=TMin;
		TMin=TMax;
		TMax=T;
	}
		
	/* Loop through threshold T values to find the best */
	for(T=TMin; T<=TMax; T++)
	{	
		tp=tn=fp=fn=0;
		
		/* read image */
		if ((fpt=fopen(FileName,"r")) == NULL)
		{
			printf("Unable to open %s for reading\n", FileName);
			exit(0);
		}
		
		/* allocate dynamic memory for image */
		Matches=(int *)calloc(ROWS*COLS,sizeof(int));
		
		//Loop through all the values and find the matches based on the threshold
		for(i=0; i<(ROWS*COLS); i++)
		{
			if(image[i]>=T)
				Matches[i]=1;
			else
				Matches[i]=0;
		}
		
		/* Create truth table for if a letter is detected and if it is the desired letter */
		while (1)
		{
			system = 0;
			i=fscanf(fpt,"%s %d %d",&gt, &GTCol, &GTRow);		//Scan the letter and coordinates of each line
			if (i != 3)
				break;																				//Break if the end is reached
			//Check the 9 x 15 pixel area to check if any pixel in the msf image is > than the threshold
			for(r=GTRow-Wr; r<GTRow+Wr; r++)
			{
				for(c=GTCol-Wc; c<GTCol+Wc; c++)
				{
					//Indicate if the system response detects the letter
					if(Matches[r*COLS+c]==1)
					{
						system = 1;
						break;						//Break out of for loop for time sake
					}
				}
			} 
			
			if (gt == SearchedLetter  &&  system == 1) tp++;
			else if (gt != SearchedLetter  &&  system == 0) tn++;
			else if (gt == SearchedLetter  &&  system == 0) fn++;
			else if (gt != SearchedLetter  &&  system == 1) fp++;
		}
		fclose(fpt);
		//printf("TP\tTN\tFP\tFN\tTPR\t\tFPR\t\tPPV\n");
		fprintf(ROCTable, "%d,%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf\n",T,tp,tn,fp,fn,
			(double)tp/(double)(tp+fn),(double)fp/(double)(fp+tn),(double)fp/(double)(tp+fp),
			(double)tp/(double)(tp+fn),1.0-(double)fp/(double)(fp+tn));
	}
	fclose(ROCTable);

}





