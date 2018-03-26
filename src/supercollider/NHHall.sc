NHHall : MultiOutUGen {
    *ar {
        |
            in,
            rt60 = 1,
            stereo = 0.5,
            lowFreq = 200,
            lowRatio = 0.5,
            hiFreq = 4000,
            hiRatio = 0.5,
            earlyDiff,
            earlyModRate,
            earlyModDepth,
            lateDiff,
            lateModRate,
            lateModDepth
        |
        in = in.asArray;
        if(in.size != 2) {
            Error(
                "NHHall expects a stereo signal, but received a size % array."
            ).throw;
        };
        ^this.multiNew(
            \audio,
            in[0],
            in[1],
            rt60,
            stereo,
            lowFreq,
            lowRatio,
            hiFreq,
            hiRatio
        );
    }

    init { arg ... theInputs;
        inputs = theInputs;
        ^this.initOutputs(2, rate);
    }

    checkInputs { ^this.checkNInputs(2) }
}
