
function median(c,v,j) { 
    asort(v,j); 
    if (c % 2) return j[(c+1)/2]; 
    else return (j[c/2+1]+j[c/2])/2.0; 
} 

BEGIN {
    OFS = "\t"
}

{
    latency+=$3;
    gbps+=$6;

    latencyValues[NR]=$3;
    gbpsValues[NR]=$6;

    latencySum+=$3; latencySumSquare+=$3*$3;
    gbpsSum+=$6; gbpsSumSquare+=$6*$6;
}

END {
    #Latency
    print   latency/NR, median(NR,latencyValues),   sqrt(latencySumSquare/NR - (latencySum/NR)**2),
            gbps/NR,    median(NR,gbpsValues),      sqrt(gbpsSumSquare/NR - (gbpsSum/NR)**2),
            losses;
}
