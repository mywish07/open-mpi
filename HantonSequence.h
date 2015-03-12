/* 
 * File:   HantonSequence.h
 * Author: novpla
 *
 * Created on November 24, 2014, 6:13 PM
 */

#ifndef HANTONSEQUENCE_H
#define	HANTONSEQUENCE_H
class HaltonSequence{
public: 
    const int base[2] = {2,3};
    const int len[2] = {63,40};
    HaltonSequence(long startIndex);
    double* nextPoint();
private:
    long index; 
    double* value;
    double** q;
    int** digit;
};


#endif	/* HANTONSEQUENCE_H */

