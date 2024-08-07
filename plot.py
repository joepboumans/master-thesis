import matplotlib.pyplot as plt
import pandas as pd
import glob
import re
import math


plot_dir = "plots/"

def plot_pds_stats(df, title, n_stage, n_struct, sz, dataset):
    # Setup subplots
    fig, axs = plt.subplots(2, 1, figsize=(9,9))
    plt.subplots_adjust(wspace=0.5, hspace=0.5)
    sz = int(sz)/1024
    fig.suptitle(f"{n_stage}.{n_struct} {title} {sz}KB\n {dataset}")

    # Plot data
    axs[0].set_title("Performance metrics", fontsize=10)
    df.plot(x = 'epoch', y = ["Recall", "Precision", "F1"], ax=axs[0])
    axs[1].set_title("Recordings", fontsize=10)
    df.plot(x = 'epoch', y = ["Total uniques", "Total found", "FP", "FN"], ax=axs[1])


    # Store figures 
    plt.savefig(f"{plot_dir}{dataset}_{n_stage}_{n_struct}_{title}_{sz}.png", transparent=True)


def parse_total_results(dfs, columns):
    scores = {}
    for column in columns:
        score = pd.DataFrame()
        for name, df in dfs.items():
            reg_name = r"(?P<name>\d+_\d+_\w+_\d+)"
            match = re.search(reg_name, name)
            if not match:
                print(f"[ERROR] Cannot parse name, skipping data {name}")
                continue;
            name = match.group("name")

            idf = pd.DataFrame(df[column])
            idf = pd.DataFrame.rename(idf, columns={column:name})
            score = pd.concat([score.reset_index(drop=True), idf.reset_index(drop=True)], ignore_index=True, sort=False)

        clean_scores = pd.DataFrame(columns=score.columns)
        for c in score.columns:
            clean_scores[c] = score[c].dropna().reset_index(drop=True)
        score = clean_scores
        score = score.reindex(sorted(score.columns, reverse=True), axis=1)
        scores[column]= score

    return scores

if __name__ == "__main__":
    # Get results
    csv_results = glob.glob("results/*.csv")

    total_df = {}
    columns = []
    # Parse results
    reg_string = r"\/(?P<data_name>.+)_(?P<name>\w+)_(?P<n_stage>\d+)_(?P<n_struct>\d+)_(?P<sz>\d+)"
    for result in csv_results:
        match = re.search(reg_string, result)
        if not match:
            print(f"[ERROR] Cannot parse name, skipping file {result}")
            continue;
        if match:
            data_info = match.groupdict()
            df = pd.read_csv(result) # epoch,total data,total pds,false pos,false neg,recall,precision,f1
            plot_pds_stats(df, data_info['name'], data_info['n_stage'], data_info['n_struct'], data_info['sz'], data_info['data_name'])
            total_df[f"{data_info['n_stage']}_{data_info['n_struct']}_{data_info['name']}_{data_info['sz']}_{data_info['data_name']}"] = df
            columns = df.columns


    scores = parse_total_results(total_df, columns)
    scores.pop('epoch')
    rows = len(scores) #math.ceil(len(scores)  / 3)
    fig2, axes = plt.subplots(rows, 1, figsize=(5,13))
    plt.subplots_adjust(wspace=0.5, hspace=0.5)

    for (param, score),i in zip(scores.items(), range(len(scores))):
        if param == "epoch":
            continue
        # row = math.floor(i / 3)
        ax = axes[i]
        ax.set_title(param)
        score.plot.box(ax=ax, vert=False, grid=True)
        ax.set_yticklabels(ax.get_yticklabels(), fontsize=8)
        # plt.title(param)
    plt.tight_layout()
    plt.savefig(f"{plot_dir}overview.png", transparent=True)
    plt.show()
