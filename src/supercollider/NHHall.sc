NHHall : MultiOutUGen {
    *ar { |in|
        in = in.asArray;
        ^this.multiNew('audio', in.first, in.last);
    }

    init { arg ... theInputs;
        inputs = theInputs;
        ^this.initOutputs(2, rate);
    }
}
