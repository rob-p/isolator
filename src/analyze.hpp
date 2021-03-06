
#ifndef ISOLATOR_ANALYZE_HPP
#define ISOLATOR_ANALYZE_HPP

#include <boost/numeric/ublas/matrix.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdio>

#include "hdf5.hpp"
#include "fragment_model.hpp"
#include "queue.hpp"
#include "sampler.hpp"
#include "transcripts.hpp"

class SamplerTickThread;
class ConditionMeanShapeSamplerThread;
class ExperimentMeanShapeSamplerThread;
class GammaBetaSampler;
class AlphaSampler;
class BetaSampler;
class GammaNormalSigmaSampler;
class GammaStudentTSigmaSampler;
class ConditionSpliceMuSigmaEtaSamplerThread;
class ExperimentSpliceMuSigmaSamplerThread;
class GammaShapeSampler;
typedef std::pair<int, int> IdxRange;


class Analyze
{
    public:
        Analyze(unsigned int rng_seed,
                size_t burnin,
                size_t num_samples,
                TranscriptSet& ts,
                const char* genome_filename,
                bool run_seqbias_correction,
                bool run_gc_correction,
                bool run_3p_correction,
                bool run_frag_correction,
                bool collect_qc_data,
                bool nopriors,
                std::set<std::string> excluded_seqs,
                std::set<std::string> bias_training_seqnames,
                double experiment_shape_alpha,
                double experiment_shape_beta,
                double experiment_splice_sigma_alpha,
                double experiment_splice_sigma_beta,
                double condition_shape_alpha,
                double condition_shape_beta_a,
                double condition_shape_beta_b,
                double condition_splice_alpha,
                double condition_splice_beta_a,
                double condition_splice_beta_b);
        ~Analyze();

        // Add a replicate under a particular condition
        void add_sample(const char* condition_name,
                        const char* filename);

        void run(hid_t file_id, bool dryrun);
        void cleanup();

    private:
        void setup_samplers();
        void setup_output(hid_t output_file_id);
        void sample(bool optimize_state);
        void write_output(size_t sample_num);

        void qsampler_update_hyperparameters();

        void compute_ts();
        void compute_xs();

        void choose_initial_values();
        void compute_scaling();

        // number of burnin samples
        size_t burnin;

        // number of samples to generate
        size_t num_samples;

        // transcript set
        TranscriptSet& transcripts;

        // File name of a fasta file containing the reference genome sequence
        // against which the reads are aligned.
        const char* genome_filename;

        // True if SeqBias correction should be used.
        bool run_seqbias_correction;

        // True if GC content correction should be used.
        bool run_gc_correction;

        // True if 3' bias should be corrected
        bool run_3p_correction;

        // True if fragmentation bias should be corrected
        bool run_frag_correction;

        // Sequences on which aligned reads should be ignored
        std::set<std::string> excluded_seqs;

        // If non-empty, contains names of sequence to which bias training
        // should be restricted.
        std::set<std::string> bias_training_seqnames;

        // True if extra extra QC data should be collected
        bool collect_qc_data;

        // True if priors should not be applie during quantification
        bool nopriors;

        // file names for the BAM/SAM file corresponding to each
        std::vector<std::string> filenames;

        // condition index to sample indexes
        std::vector<std::vector<int> > condition_samples;

        // fragment models for each sample
        std::vector<FragmentModel*> fms;

        // quantification samplers for each sample
        std::vector<Sampler*> qsamplers;

        // threads used for iterating samplers
        std::vector<SamplerTickThread*> qsampler_threads;
        std::vector<ConditionMeanShapeSamplerThread*> meanshape_sampler_threads;
        std::vector<ExperimentMeanShapeSamplerThread*> experiment_meanshape_sampler_threads;
        GammaBetaSampler* gamma_beta_sampler;
        BetaSampler* invgamma_beta_sampler;
        GammaNormalSigmaSampler* gamma_normal_sigma_sampler;
        GammaShapeSampler* gamma_shape_sampler;

        std::vector<ConditionSpliceMuSigmaEtaSamplerThread*> splice_mu_sigma_sampler_threads;
        std::vector<ExperimentSpliceMuSigmaSamplerThread*>
            experiment_splice_mu_sigma_sampler_threads;

        // queues to send work to sampler threads, and be notified on completion
        // of ticks.
        Queue<int> qsampler_tick_queue, qsampler_notify_queue;

        // work is doled out in block for these. Otherwise threads can starve
        // when there are few sample in the experiment
        Queue<IdxRange> meanshape_sampler_tick_queue,
                        experiment_meanshape_sampler_tick_queue,
                        splice_mu_sigma_sampler_tick_queue,
                        experiment_splice_mu_sigma_sampler_tick_queue;

        Queue<int> meanshape_sampler_notify_queue,
                   experiment_meanshape_sampler_notify_queue,
                   splice_mu_sigma_sampler_notify_queue,
                   experiment_splice_mu_sigma_sampler_notify_queue;

        // We maintain a different rng for every unit of work for threads.
        // That way we can actually make isolator run reproducible.
        std::vector<rng_t> transcript_rng_pool;
        std::vector<rng_t> splice_rng_pool;

        // matrix containing relative transcript abundance samples, indexed by:
        //   sample -> transcript (tid)
        boost::numeric::ublas::matrix<float> Q;

        // transcript mean parameter, indexed by condition -> transcript
        boost::numeric::ublas::matrix<float> condition_mean;

        // transcript shape parameter, indexed by transcript
        std::vector<float> condition_shape;

        // parameters of the inverse gamma prior on condition_splice_sigma
        double condition_splice_alpha, condition_splice_beta;

        // parameters of the inverse gamma prior on condition_shape
        double condition_shape_alpha, condition_shape_beta;

        // experiment-wise transcript position paremeter, indexed by transcript
        std::vector<float> experiment_mean;

        // experiment-wide transcript scale parameter
        double experiment_shape;

        // gamma hypeparameters for prior on experiment_shape
        double experiment_shape_alpha;
        double experiment_shape_beta;

        // parameters for normal prior over experiment_mean
        double experiment_mean0, experiment_shape0;

        // tids belonging to each tgroup (indexed by tgroup)
        std::vector<std::vector<unsigned int> > tgroup_tids;

        // sorted indexes of tgroups with multiple transcripts
        std::vector<unsigned int> spliced_tgroup_indexes;

        // condition slice mean indexed by condition, spliced tgroup, transcript
        // according to spliced_tgroup_indexes and tgroup_tids
        std::vector<std::vector<std::vector<float> > > condition_splice_mu;

        // per-spliced-tgroup experiment wide logistic-normal mean
        std::vector<std::vector<float> > experiment_splice_mu;

        // prior parameters for experimient_splice_mu
        double experiment_splice_nu, experiment_splice_mu0, experiment_splice_sigma0;

        // experiment std. dev.
        double experiment_splice_sigma;

        // gamma hypeparameters for prior on experiment_splice_sigma
        double experiment_splice_sigma_alpha;
        double experiment_splice_sigma_beta;

        // splicing precision, indexed by spliced tgroup
        std::vector<std::vector<float> > condition_splice_sigma;

        // overparameterization to unstick stuck samplers
        std::vector<std::vector<float> > condition_splice_eta;

        // flattened condition_splice_sigma used for sampling alpha, beta
        // params.
        std::vector<float> condition_splice_sigma_work;
        std::vector<float> experiment_splice_sigma_work;

        // paramaters for the inverse gamma priors on splice_alpha and
        // splice_beta
        double condition_splice_beta_a,
               condition_splice_beta_b;

        // Condition index corresponding to the given name
        std::map<std::string, int> condition_index;

        // Condition index of sample i
        std::vector<int> condition;

        // normalization constant for each sample
        std::vector<float> scale;

        // temporary space used for computing scale
        std::vector<float> scale_work;

        // number of sequenced samples
        unsigned int K;

        // number of conditions
        unsigned int C;

        // number of transcripts
        unsigned int N;

        // number of tgroups
        unsigned int T;

        // Hyperparams for inverse gamma prior on tgroup_alpha/tgroup_beta
        double condition_shape_beta_a,
               condition_shape_beta_b;

        // RNG used for alpha/beta samplers
        unsigned int rng_seed;
        rng_t rng;

        // HDF5 dataspace ids, for output purposes

        // dataspaces
        hid_t h5_experiment_mean_dataspace;
        hid_t h5_condition_mean_dataspace;
        hid_t h5_condition_mean_mem_dataspace;
        hid_t h5_row_mem_dataspace;
        hid_t h5_sample_quant_dataspace;
        hid_t h5_sample_quant_mem_dataspace;
        hid_t h5_experiment_splice_dataspace;
        hid_t h5_condition_splice_mu_dataspace;
        hid_t h5_condition_splice_sigma_dataspace;
        hid_t h5_splicing_mem_dataspace;
        hid_t h5_sample_scaling_dataspace;
        hid_t h5_sample_scaling_mem_dataspace;

        // datasets
        hid_t h5_experiment_mean_dataset;
        hid_t h5_condition_mean_dataset;
        hid_t h5_condition_shape_dataset;
        hid_t h5_sample_quant_dataset;
        hid_t h5_experiment_splice_mu_dataset;
        hid_t h5_experiment_splice_sigma_dataset;
        hid_t h5_condition_splice_mu_dataset;
        hid_t h5_condition_splice_sigma_dataset;
        hid_t h5_sample_scaling_dataset;

        // variable length array for splicing paramaters
        hid_t h5_splice_param_type;

        // structure for the ragged-array splicing data
        hvl_t* h5_splice_work;

        // a write buffer for hdf5 output
        std::vector<float> row_data;

        friend void write_qc_data(FILE* fout, Analyze& analyze);
        friend void compare_seqbias(Analyze& analyze, TranscriptSet& ts,
                                    const char* genome_filename);
};


#endif

