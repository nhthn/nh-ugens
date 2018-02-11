NHHall : MultiOutUGen {
    *ar { |in, rt60 = 2|
        in = in.asArray;
        ^this.multiNew('audio', in.first, in.last);
    }

    init { arg ... theInputs;
        inputs = theInputs;
        ^this.initOutputs(2, rate);
    }
}
