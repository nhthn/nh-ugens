NHHall : MultiOutUGen {
    *ar { |in, rt60 = 1|
        in = in.asArray;
        ^this.multiNew('audio', in.first, in.last, rt60);
    }

    init { arg ... theInputs;
        inputs = theInputs;
        ^this.initOutputs(2, rate);
    }
}
