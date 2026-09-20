clc;
clear;
close all;

SkidPad3 = table2array(readtable('Skid Pad 3.csv'));

time = SkidPad3(: , 1); % Select time values from xlsx
time = time / 1e6;      % Convert to seconds

num = numel(time); % Store number of elements in array data

% Get average time between samples
% samplingIntervals = zeros(num, 1);
% for i = 1 : (num-1)
%     samplingIntervals(i) = time(i+1) - time(i);
% end
% samplingFreqAvg = 1 / mean(samplingIntervals)

samplingFreqAvg = 1/0.005;

% Select sus travel values from xlsx
FRData = SkidPad3(: , 2) / 1024;
FLData = SkidPad3(: , 3) / 1024;
RRData = SkidPad3(: , 4) / 1024;
RLData = SkidPad3(: , 5) / 1024;

Data = [(time(2 : end) - time(2)) (FRData(2 : end))];

% Fourier Transforms
FRFFT = abs(fft(FRData));
FLFFT = abs(fft(FLData));
RRFFT = abs(fft(RRData));
RLFFT = abs(fft(RLData));

nyFreq = samplingFreqAvg / 2;
frequencies = linspace(0, nyFreq, num); % Create array of frequency values


out = sim("Anti_Aliasing_Filter.slx");

plotMode = 1;

if plotMode == 0
    subplot(2, 6, 1)
    plot(time, FRData, 'g')
    title('FRData')
    ylim([min(FRData) max(FRData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 2)
    plot(time, FLData, 'b')
    title('FLData')
    ylim([min(FLData) max(FLData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 3)
    plot(time, RRData, 'r')
    title('RRData')
    ylim([min(RRData) max(RRData)])
    xlim([time(1) time(end)])
    
    subplot(2, 6, 4)
    plot(time, RLData, 'y')
    title('RLData')
    ylim([min(RLData) max(RLData)])
    xlim([time(1) time(end)])


    subplot(2, 6, 7)
    plot(frequencies, FRFFT, 'g')
    title('FRFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 8)
    plot(frequencies, FLFFT, 'b')
    title('FLFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 9)
    plot(frequencies, RRFFT, 'r')
    title('RRFFT')
    set(gca, 'YScale', 'log')
    
    subplot(2, 6, 10)
    plot(frequencies, RLFFT, 'y')
    title('RLFFT')
    set(gca, 'YScale', 'log')
    
    
    
    subplot(2, 6, [5,6,11,12])
    plot(frequencies, FRFFT, 'g')
    hold on
    plot(frequencies, FLFFT, 'b')
    plot(frequencies, RRFFT, 'r')
    plot(frequencies, RLFFT, 'y')
    set(gca, 'YScale', 'log')
    hold off
elseif plotMode == 1

    % Original data (time & frequency domains)
    subplot(2, 3, 1)
    plot(time(2 : end), out.UnfilteredData(1 : numel(time) - 1), 'g')
    title('Unfiltered Data')
    xlabel('time (s)')
    ylabel('voltage (V)')

    subplot(2, 3, 4)
    plot(frequencies, FRFFT, 'g')
    set(gca, 'YScale', 'log')
    title('Unfiltered FFT')
    xlabel('frequency (Hz)')
    ylabel('power')


    % Filtered data (time & frequency domains)
    subplot(2, 3, 2)
    plot(time(2 : end), out.FilteredData(1 : numel(time) - 1), 'y')
    title('Filtered Data')
    xlabel('time (s)')
    ylabel('voltage (V)')

    FilteredFFT = abs(fft(out.FilteredData));

    subplot(2, 3, 5)
    plot(frequencies(2 : end), FilteredFFT(1 : numel(frequencies) - 1), 'y')
    set(gca, 'YScale', 'log') 
    title('Filtered FFT')
    xlabel('frequency (Hz)')
    ylabel('power')


    % Both compared directly
    subplot(2, 3, 3)
    plot(time(2 : end), out.UnfilteredData(1 : numel(time) - 1), 'g')
    hold on
    plot(time(2 : end), out.FilteredData(1 : numel(time) - 1), 'y')
    xlabel('time (s)')
    ylabel('voltage (V)')
    hold off

    subplot(2, 3, 6)
    plot(frequencies, FRFFT, 'g')
    set(gca, 'XScale', 'log')
    set(gca, 'YScale', 'log')
    hold on
    plot(frequencies(2 : end), FilteredFFT(1 : numel(frequencies) - 1), 'y')
    xlabel('frequency (Hz)')
    ylabel('power')
    hold off
end
